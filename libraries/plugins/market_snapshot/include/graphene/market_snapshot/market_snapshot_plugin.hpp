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
#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/market_object.hpp>

#include <fc/thread/future.hpp>

namespace graphene { namespace market_snapshot {
using namespace chain;

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef MARKET_SNAPSHOT_SPACE_ID
#define MARKET_SNAPSHOT_SPACE_ID 6
#endif

/*
 * Data structure of one market pair (for example BTS/USD):
 * * a calculation period = snapshots
 * * a snapshot = time stamp, feed price, ask orders, bid orders
 * * a order = direction(ask/bid), price, volume, owner, create_time
 */
struct market_snapshot_key
{
   market_snapshot_key( asset_id_type a, asset_id_type b, fc::time_point_sec t )
      : base(a),quote(b),snapshot_time(t) {}
   market_snapshot_key() {}

   asset_id_type      base;
   asset_id_type      quote;
   fc::time_point_sec snapshot_time;

   friend bool operator < ( const market_snapshot_key& a, const market_snapshot_key& b )
   {
      return std::tie( a.base, a.quote, a.snapshot_time ) < std::tie( b.base, b.quote, b.snapshot_time );
   }
   friend bool operator == ( const market_snapshot_key& a, const market_snapshot_key& b )
   {
      return std::tie( a.base, a.quote, b.snapshot_time ) == std::tie( b.base, b.quote, b.snapshot_time );
   }
};
struct market_snapshot_order
{
   market_snapshot_order(){}
   market_snapshot_order( const limit_order_object& o )
      : sell_price( o.sell_price ),
        for_sale( o.for_sale ),
        seller( o.seller ),
        order_id( o.id ) {}

   price               sell_price;
   share_type          for_sale;
   fc::time_point_sec  create_time;
   account_id_type     seller;
   limit_order_id_type order_id;
};
struct market_snapshot_object : public abstract_object<market_snapshot_object>
{
   static const uint8_t space_id = MARKET_SNAPSHOT_SPACE_ID;
   static const uint8_t type_id  = 0;

   market_snapshot_key           key;
   price                         feed_price;
   vector<market_snapshot_order> bids;
   vector<market_snapshot_order> asks;
};

struct by_id;
struct by_key;
typedef multi_index_container<
   market_snapshot_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_key>, member< market_snapshot_object,
                                           market_snapshot_key,
                                           &market_snapshot_object::key > >
   >
> market_snapshot_object_multi_index_type;

typedef generic_index<market_snapshot_object,
                      market_snapshot_object_multi_index_type> market_snapshot_index;


struct market_snapshot_new_order_object : public abstract_object<market_snapshot_object>
{
   static const uint8_t space_id = MARKET_SNAPSHOT_SPACE_ID;
   static const uint8_t type_id  = 1;

   price               sell_price;
   share_type          for_sale;
   fc::time_point_sec  create_time;
   account_id_type     seller;
   limit_order_id_type order_id;
};

struct by_order_id{};
typedef multi_index_container<
   market_snapshot_new_order_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_order_id>, member< market_snapshot_new_order_object,
                                               limit_order_id_type,
                                               &market_snapshot_new_order_object::order_id > >
   >
> market_snapshot_order_multi_index_type;

typedef generic_index<market_snapshot_new_order_object,
                      market_snapshot_order_multi_index_type> market_snapshot_order_index;


struct market_snapshot_config
{
   asset_id_type          base;
   asset_id_type          quote;
   fc::time_point_sec     begin_time       = fc::time_point_sec( 0 );
   uint64_t               max_seconds      = 60 * 60 * 24 * 16;
   bool                   track_ask_orders = true;
   bool                   track_bid_orders = true;
};

typedef std::pair< asset_id_type, asset_id_type >                 snapshot_market_type;
// note: use flat data types here for small sets
typedef flat_set< snapshot_market_type >                          snapshot_markets_type;
typedef flat_map< snapshot_market_type, market_snapshot_config >  snapshot_markets_config_type;

struct market_snapshot_meta_object : public abstract_object<market_snapshot_meta_object>
{
   static const uint8_t space_id = MARKET_SNAPSHOT_SPACE_ID;
   static const uint8_t type_id  = 2;

   // key
   snapshot_market_type   market;

   // config
   market_snapshot_config config;

   // stats
   fc::time_point_sec     oldest_snapshot_time;
   fc::time_point_sec     newest_snapshot_time;
   uint32_t               count;
};

typedef multi_index_container<
   market_snapshot_meta_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_key>, member< market_snapshot_meta_object,
                                           snapshot_market_type,
                                           &market_snapshot_meta_object::market > >
   >
> market_snapshot_meta_object_multi_index_type;


namespace detail
{
    class market_snapshot_plugin_impl;
}


/**
 *  The market snapshot plugin can be configured to track any market pairs.
 *  It takes a snapshot of the market status when there is a change to the market.
 */
class market_snapshot_plugin : public graphene::app::plugin
{
   public:
      market_snapshot_plugin();
      virtual ~market_snapshot_plugin();

      std::string plugin_name()const override;
      virtual void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      virtual void plugin_initialize(
         const boost::program_options::variables_map& options) override;
      virtual void plugin_startup() override;

      const snapshot_markets_config_type& tracked_markets()const;

   private:
      friend class detail::market_snapshot_plugin_impl;
      std::unique_ptr<detail::market_snapshot_plugin_impl> my;
};

} } //graphene::market_snapshot

FC_REFLECT( graphene::market_snapshot::market_snapshot_key, (base)(quote)(snapshot_time) )
FC_REFLECT( graphene::market_snapshot::market_snapshot_order,
            (sell_price)(for_sale)(create_time)(seller)(order_id) )
FC_REFLECT_DERIVED( graphene::market_snapshot::market_snapshot_object,
                    (graphene::db::object),
                    (key)(feed_price)(bids)(asks) )
FC_REFLECT_DERIVED( graphene::market_snapshot::market_snapshot_new_order_object,
                    (graphene::db::object),
                    (sell_price)(for_sale)(create_time)(seller)(order_id) )
FC_REFLECT( graphene::market_snapshot::market_snapshot_config,
                    (base)(quote)(begin_time)(max_seconds)(track_ask_orders)(track_bid_orders) )
FC_REFLECT_DERIVED( graphene::market_snapshot::market_snapshot_meta_object,
                    (graphene::db::object),
                    (market)(config)
                    (oldest_snapshot_time)(newest_snapshot_time)(count) )
