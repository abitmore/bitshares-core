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
       * and will remove tracked orders which aren't needed any more.
       */
      void update_order_index( const vector<object_id_type>& v );

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
};


market_snapshot_plugin_impl::~market_snapshot_plugin_impl()
{}

void market_snapshot_plugin_impl::update_order_index( const vector<object_id_type>& v )
{
   if( _tracked_markets.size() == 0 ) return;

   graphene::chain::database& db = database();
   for( const object_id_type& id : v )
   {
      if( id.is<limit_order_id_type>() ) // a limit order changed
      {
         const object* pobj = db.find_object( id );
         if( pobj == nullptr ) // if it's a removed object, locate it with market_snapshot_order_index
         {
            const auto& idx = db.get_index_type<market_snapshot_order_index>().indices().get<by_order_id>();
            const auto p = idx.find( id );
            if( p != idx.end() ) // found it, remove it
            {
               dlog("removing order ${o}",("o",*p));
               db.remove( *p );
            }
         }
      }
   }
}

struct market_snapshot_operation_visitor
{
   market_snapshot_plugin&    _plugin;
   const fc::time_point_sec   _now;
   const operation_result&    _op_result;

   market_snapshot_operation_visitor( market_snapshot_plugin& p,
                                      fc::time_point_sec t,
                                      const operation_result& r )
   :_plugin(p),_now(t),_op_result(r) {}

   typedef optional<snapshot_market_type> result_type;

   /** do nothing for other operation types */
   template<typename T>
   result_type operator()( const T& )const{ return result_type(); }

   result_type operator()( const limit_order_create_operation& op )const
   {
      //dlog( "processing ${op}", ("op",op) );
      auto& db = _plugin.database();
      const auto& tracked_markets = _plugin.tracked_markets();
      const auto& market = op.get_market();

      if( tracked_markets.find( market ) == tracked_markets.end() )
         return result_type();

      // there is a new order, record it
      db.create<market_snapshot_new_order_object>( [&]( market_snapshot_new_order_object& order ){
         order.order_id = _op_result.get<object_id_type>();
         order.seller = op.seller;
         order.create_time = _now;
         order.for_sale = op.amount_to_sell.amount;
         order.sell_price = op.get_price();
         idump((order));
      });

      return market;
   }

   result_type operator()( const limit_order_cancel_operation& op )const
   {
      //dlog( "processing ${op}", ("op",op) );
      auto& db = _plugin.database();
      const auto& idx = db.get_index_type<market_snapshot_order_index>().indices().get<by_order_id>();
      const auto p = idx.find( op.order );
      // if the order is in new_order database, the market is tracked
      if( p != idx.end() )
         return( p->sell_price.get_market() );
      else
         return result_type();
   }

   result_type operator()( const fill_order_operation& op )const
   {
      //dlog( "processing ${op}", ("op",op) );
      const auto& tracked_markets = _plugin.tracked_markets();
      const auto& market = op.get_market();
      if( tracked_markets.find( market ) != tracked_markets.end() )
         return market;
      else
         return result_type();
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

   // check if there is order changed, and record new orders
   flat_set<snapshot_market_type> changed_markets;
   const vector<optional< operation_history_object > >& hist = db.get_applied_operations();
   idump((hist));
   for( const optional< operation_history_object >& o_op : hist )
   {
      if( o_op.valid() )
      {
         market_snapshot_operation_visitor v( _self, b.timestamp, o_op->result );
         const optional<snapshot_market_type>& result = o_op->op.visit( v );
         if( result.valid() )
            changed_markets.insert( *result );
      }
   }

   // take snapshots
   for( const auto& m : _tracked_markets )
   {
      const auto& market = m.first;
      const auto& config = m.second;

      //TODO remove old snapshots from the database

      if( b.timestamp < config.begin_time )
      {
         //TODO erase snapshots before begin_time?
         continue;
      }

      // get feed_price
      price feed_price;
      bool found_feed_price = false;
      const asset_object& quote_obj = config.quote( db );
      if( quote_obj.is_market_issued() )
      {
         const auto& bitasset_id = *quote_obj.bitasset_data_id;
         const asset_bitasset_data_object& bo = bitasset_id( db );
         if( bo.options.short_backing_asset == config.base )
         {
            found_feed_price = true;
            feed_price = bo.current_feed.settlement_price;
         }
      }
      if( !found_feed_price )
      {
         const asset_object& base_obj = config.base( db );
         if( base_obj.is_market_issued() )
         {
            const auto& bitasset_id = *base_obj.bitasset_data_id;
            const asset_bitasset_data_object& bo = bitasset_id( db );
            if( bo.options.short_backing_asset == config.quote )
            {
               found_feed_price = true;
               feed_price = bo.current_feed.settlement_price;
            }
         }
      }

      // get statistics
      const auto& stat_idx = db.get_index_type<market_snapshot_statistics_index>().indices().get<by_key>();
      const auto stat_itr = stat_idx.find( market );
      bool new_stat = ( stat_itr == stat_idx.end() );

      // check whether need to take snapshot
      if( !new_stat && changed_markets.find( market ) == changed_markets.end() )
      {
         // if not first snapshot, and no order change

         if( !found_feed_price ) // if feed_price does not apply
            continue;

         if( feed_price == stat_itr->newest_feed_price ) // if feed_price didn't change
            continue;
      }

      // init key
      market_snapshot_key key( config.base, config.quote, b.timestamp );

      // init orders
      vector<market_snapshot_order> bids;
      vector<market_snapshot_order> asks;

      const auto& limit_order_idx = db.get_index_type<limit_order_index>();
      const auto& limit_price_idx = limit_order_idx.indices().get<by_price>();
      const auto& snapshot_order_idx = db.get_index_type<market_snapshot_order_index>()
                                         .indices().get<by_id>();

      if( config.track_bid_orders )
      {
         auto limit_itr = limit_price_idx.lower_bound(price::max(config.base, config.quote));
         auto limit_end = limit_price_idx.upper_bound(price::min(config.base, config.quote));
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
         auto limit_itr = limit_price_idx.lower_bound(price::max(config.quote, config.base));
         auto limit_end = limit_price_idx.upper_bound(price::min(config.quote, config.base));
         while( limit_itr != limit_end )
         {
            market_snapshot_order order( *limit_itr );
            const auto p = snapshot_order_idx.find( order.order_id );
            if( p != snapshot_order_idx.end() ) order.create_time = p->create_time;
            asks.push_back( order );
            ++limit_itr;
         }
      }

      // store snapshot
      db.create<market_snapshot_object>( [&]( market_snapshot_object& mso ){
           mso.key = key;
           mso.feed_price = feed_price;
           mso.asks = asks;
           mso.bids = bids;
           idump((mso));
      });

      // store statistics
      if( new_stat )
      {
         db.create<market_snapshot_statistics_object>( [&]( market_snapshot_statistics_object& msso ){
              msso.market = market;
              msso.oldest_snapshot_time = b.timestamp;
              msso.newest_snapshot_time = b.timestamp;
              msso.newest_feed_price = feed_price;
              msso.count = 1;
              idump((msso));
         });
      }
      else
      {
         db.modify( *stat_itr, [&]( market_snapshot_statistics_object& msso ){
              msso.newest_snapshot_time = b.timestamp;
              msso.newest_feed_price = feed_price;
              msso.count += 1;
              idump((msso));
         });
      }

   }
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
   database().applied_block.connect( [&]( const signed_block& b){ my->take_market_snapshots(b); } );
   database().changed_objects.connect( [&]( const vector<object_id_type>& v){ my->update_order_index(v); } );
   database().add_index< primary_index< market_snapshot_index  > >();
   database().add_index< primary_index< market_snapshot_order_index  > >();
   database().add_index< primary_index< market_snapshot_statistics_index  > >();

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
   idump((my->_tracked_markets));
} FC_CAPTURE_AND_RETHROW() }

void market_snapshot_plugin::plugin_startup()
{
}

const snapshot_markets_config_type& market_snapshot_plugin::tracked_markets() const
{
   return my->_tracked_markets;
}

} }
