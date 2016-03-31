/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <graphene/market_snapshot/market_snapshot_plugin.hpp>

#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/operation_history_object.hpp>

#include <fc/thread/thread.hpp>
#include <fc/smart_ref_impl.hpp>

namespace graphene { namespace market_snapshot {

namespace detail
{

class market_snapshot_plugin_impl
{
   public:
      market_snapshot_plugin_impl(market_snapshot_plugin& _plugin)
      :_self( _plugin ) {}
      virtual ~market_snapshot_plugin_impl();

      /**
       * This method is called as a callback after a change is made to chain database
       * and will check whether need to take a snapshot after a block is applied.
       */
      void update_changed_markets( const vector<object_id_type>& v );

      /**
       * This method is called as a callback after a block is applied
       * and will take a snapshot of markets whose status is changed in the block.
       */
      void take_market_snapshots( const signed_block& b );

      graphene::chain::database& database()
      {
         return _self.database();
      }

      market_snapshot_plugin&          _self;
      snapshot_markets_config_type     _tracked_markets;
   private:
      snapshot_markets_type            _changed_markets;
      uint64_t count = 0;
};


market_snapshot_plugin_impl::~market_snapshot_plugin_impl()
{}


void market_snapshot_plugin_impl::update_changed_markets( const vector<object_id_type>& v )
{
   if( _tracked_markets.size() == 0 ) return;

   graphene::chain::database& db = database();
   for( const object_id_type& id : v )
   {
      if( id.is<limit_order_id_type>() ) // a limit order changed
      {
         const object* pobj = db.find_object( id );
         if( pobj != nullptr ) // if it's not a removed object
         {
            const limit_order_object* order = static_cast< const limit_order_object* >( pobj );
            const snapshot_market_type& market = order->get_market();
            if( _tracked_markets.find( market ) != _tracked_markets.end() )
               _changed_markets.insert( market );
         }
         else // if it's a removed object, try to locate it with market_snapshot_order_index
         {
            const auto& idx = db.get_index_type<market_snapshot_order_index>().indices().get<by_order_id>();
            const auto p = idx.find( id );
            if( p != idx.end() )
            {
               // if the order is in new_order database, the market is tracked
               _changed_markets.insert( p->sell_price.get_market() );
            }
         }
      }
      else if( id.is<asset_bitasset_data_id_type>() ) // a bit_asset object changed, perhaps feed_price changed
      {
         //TODO possible optimization: don't take snapshot when only settlement_volume changes
         const asset_bitasset_data_object& obj = db.get( id.as<asset_bitasset_data_id_type>() );
         const snapshot_market_type& market = obj.current_feed.settlement_price.get_market();
         if( _tracked_markets.find( market ) != _tracked_markets.end() )
            _changed_markets.insert( market );
      }
   }
}

// when there is a new order, record it
struct snapshot_new_order_visitor
{
   market_snapshot_plugin&    _plugin;
   const fc::time_point_sec   _now;
   const operation_result&    _op_result;

   snapshot_new_order_visitor( market_snapshot_plugin& p, fc::time_point_sec t, const operation_result& r )
   :_plugin(p),_now(t),_op_result(r) {}

   typedef void result_type;

   /** do nothing for other operation types */
   template<typename T>
   void operator()( const T& )const{}

   void operator()( const limit_order_create_operation& op )const
   {
      dlog( "processing ${op}", ("op",op) );
      auto& db         = _plugin.database();
      const auto& tracked_markets = _plugin.tracked_markets();
      const auto& market = op.get_market();

      if( tracked_markets.find( market ) == tracked_markets.end() )
         return;

      db.create<market_snapshot_new_order_object>( [&]( market_snapshot_new_order_object& order ){
         order.order_id = _op_result.get<object_id_type>();
         order.seller = op.seller;
         order.create_time = _now;
         order.for_sale = op.amount_to_sell.amount;
         order.sell_price = op.get_price();
      });
   }
};

void market_snapshot_plugin_impl::take_market_snapshots( const signed_block& b )
{
   //idump(( b.block_num() ));
   if( _tracked_markets.size() == 0 ) return;

   graphene::chain::database& db = database();

   //TODO need to remove data recorded with old forks whose time >= b.timestamp ??
   //     primary index will automatically rollback while switching to a new fork ??
   /*{
      const auto& order_time_idx = db.get_index_type<market_snapshot_order_index>().indices().get<by_time>();
      auto order_itr = order_time_idx.lower_bound( b.timestamp );
      while( order_itr.valid() )
      {
         db.remove( *(order_itr++) );
      }
   }*/

   // TODO remove old snapshots from the database

   // TODO remove orders from the database which are too old (disappeared before oldest snapshot)

   if( _changed_markets.size() == 0 ) return;

   // record new orders
   const vector<optional< operation_history_object > >& hist = db.get_applied_operations();
   for( const optional< operation_history_object >& o_op : hist )
   {
      if( o_op.valid() )
         o_op->op.visit( snapshot_new_order_visitor( _self, b.timestamp, o_op->result ) );
   }

   // take snapshots
   for( const snapshot_market_type& m : _changed_markets )
   {
      const auto& config = _tracked_markets.at( m );
      if( b.timestamp < config.begin_time ) continue;

      // init key
      market_snapshot_key key( m.first, m.second, b.timestamp );

      // init feed_price
      price feed_price;
      bool found_feed_price = false;
      const asset_object& quote_obj = m.second( db );
      if( quote_obj.is_market_issued() )
      {
         const auto& bid = *quote_obj.bitasset_data_id;
         const asset_bitasset_data_object& bo = bid( db );
         if( bo.options.short_backing_asset == m.first )
         {
            found_feed_price = true;
            feed_price = bo.current_feed.settlement_price;
         }
      }
      if( !found_feed_price )
      {
         const asset_object& base_obj = m.first( db );
         if( base_obj.is_market_issued() )
         {
            const auto& bid = *base_obj.bitasset_data_id;
            const asset_bitasset_data_object& bo = bid( db );
            if( bo.options.short_backing_asset == m.second )
            {
               found_feed_price = true;
               feed_price = bo.current_feed.settlement_price;
            }
         }
      }

      // init orders
      vector<market_snapshot_order> bids;
      vector<market_snapshot_order> asks;

      const auto& limit_order_idx = db.get_index_type<limit_order_index>();
      const auto& limit_price_idx = limit_order_idx.indices().get<by_price>();
      const auto& snapshot_order_idx = db.get_index_type<market_snapshot_order_index>()
                                         .indices().get<by_id>();

      if( config.track_bid_orders )
      {
         auto limit_itr = limit_price_idx.lower_bound(price::max(m.first, m.second));
         auto limit_end = limit_price_idx.upper_bound(price::min(m.first, m.second));
         while( limit_itr != limit_end )
         {
            market_snapshot_order order( *limit_itr );
            const auto p = snapshot_order_idx.find( order.order_id );
            if( p != snapshot_order_idx.end() ) order.create_time = p->create_time;
            bids.push_back( order );
            ++limit_itr;
         }
      }

      if( config.track_ask_orders )
      {
         auto limit_itr = limit_price_idx.lower_bound(price::max(m.second, m.first));
         auto limit_end = limit_price_idx.upper_bound(price::min(m.second, m.first));
         while( limit_itr != limit_end )
         {
            market_snapshot_order order( *limit_itr );
            const auto p = snapshot_order_idx.find( order.order_id );
            if( p != snapshot_order_idx.end() ) order.create_time = p->create_time;
            asks.push_back( order );
            ++limit_itr;
         }
      }

      db.create<market_snapshot_object>( [&]( market_snapshot_object& mso ){
           mso.key = key;
           mso.feed_price = feed_price;
           mso.asks = asks;
           mso.bids = bids;
      });

      count++;
   }
   // clear changed markets
   _changed_markets.clear();
}

} // end namespace detail


market_snapshot_plugin::market_snapshot_plugin() :
   my( new detail::market_snapshot_plugin_impl(*this) )
{
}

market_snapshot_plugin::~market_snapshot_plugin()
{
}

std::string market_snapshot_plugin::plugin_name()const
{
   return "market_snapshot";
}

void market_snapshot_plugin::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg
   )
{
   cli.add_options()
         ("market-snapshot-options", boost::program_options::value<string>()->default_value("[]"),
          "Market snapshot options. Syntax: [{\"base\":base_asset_id,\"quote\":quote_asset_id,\"begin_time\":begin_time,\"max_seconds\":max_seconds,\"track_ask_orders\":is_track_asks,\"track_bid_orders\":is_track_bids},...] . Example: [{\"base\":\"1.3.0\",\"quote\":\"1.3.1\",\"begin_time\":\"2016-03-01T00:00:00\",\"max_seconds\":864000,\"track_ask_orders\":true,\"track_bid_orders\":true},{\"base\":\"1.3.0\",\"quote\":\"1.3.2\",\"begin_time\":\"2016-01-01T00:00:00\",\"max_seconds\":4320000,\"track_ask_orders\":false,\"track_bid_orders\":false}]")
         ;
   cfg.add(cli);
}

void market_snapshot_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
   database().changed_objects.connect( [&]( const vector<object_id_type>& v){ my->update_changed_markets(v); } );
   // TODO there is a bug which causes the connected funtion not always be called in time, therefore results in a wrong snapshot.
   database().applied_block.connect( [&]( const signed_block& b){ my->take_market_snapshots(b); } );
   database().add_index< primary_index< market_snapshot_index  > >();
   database().add_index< primary_index< market_snapshot_order_index  > >();

   if( options.count( "market-snapshot-options" ) )
   {
      const std::string& markets = options["market-snapshot-options"].as<string>();
      vector<market_snapshot_config> v = fc::json::from_string(markets).as< vector<market_snapshot_config> >();
      for( market_snapshot_config& c : v )
      {
         if( c.base > c.quote )
         {
            std::swap( c.base, c.quote );
            std::swap( c.track_bid_orders, c.track_ask_orders);
         }
         snapshot_market_type key = std::make_pair( c.base, c.quote );
         my->_tracked_markets.insert( std::make_pair( key, c ) );
      }
   }
} FC_CAPTURE_AND_RETHROW() }

void market_snapshot_plugin::plugin_startup()
{
}

const snapshot_markets_config_type& market_snapshot_plugin::tracked_markets() const
{
   return my->_tracked_markets;
}

} }
