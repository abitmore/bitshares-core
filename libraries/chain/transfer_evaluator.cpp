/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Any modified source or binaries are used only with the BitShares network.
 *
 * 2. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 3. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <graphene/chain/transfer_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {
void_result transfer_evaluator::do_evaluate( const transfer_operation& op )
{ try {
   
   const database& d = db();

   const account_object& from_account    = op.from(d);
   const account_object& to_account      = op.to(d);
   const asset_object&   asset_type      = op.amount.asset_id(d);
   const asset_object&   fee_asset_type  = op.fee.asset_id(d);

   try {

      share_type required_core_fee = d.current_fee_schedule().calculate_fee( op, asset_type ).amount;
      GRAPHENE_ASSERT( core_fee_paid + max_fees_payable_with_coin_seconds >= required_core_fee,
                    insufficient_fee,
                    "Insufficient Fee Paid",
                    ("core_fee_paid",core_fee_paid)("required",required_core_fee) );
      // if some fees are paid with coin seconds
      if( core_fee_paid < required_core_fee )
         fees_paid_with_coin_seconds = required_core_fee - core_fee_paid;

      if( asset_type.options.flags & white_list )
      {
         GRAPHENE_ASSERT(
            from_account.is_authorized_asset( asset_type, d ),
            transfer_from_account_not_whitelisted,
            "'from' account ${from} is not whitelisted for asset ${asset}",
            ("from",op.from)
            ("asset",op.amount.asset_id)
            );
         GRAPHENE_ASSERT(
            to_account.is_authorized_asset( asset_type, d ),
            transfer_to_account_not_whitelisted,
            "'to' account ${to} is not whitelisted for asset ${asset}",
            ("to",op.to)
            ("asset",op.amount.asset_id)
            );
      }

      if( d.head_block_time() <= HARDFORK_419_TIME )
      {
         if( fee_asset_type.options.flags & white_list )
            FC_ASSERT( from_account.is_authorized_asset( asset_type, d ) );
      }
      // the above becomes no-op after hardfork because this check will then be performed in evaluator

      if( asset_type.is_transfer_restricted() )
      {
         GRAPHENE_ASSERT(
            from_account.id == asset_type.issuer || to_account.id == asset_type.issuer,
            transfer_restricted_transfer_asset,
            "Asset {asset} has transfer_restricted flag enabled",
            ("asset", op.amount.asset_id)
          );
      }

      bool insufficient_balance = d.get_balance( from_account, asset_type ).amount >= op.amount.amount;
      FC_ASSERT( insufficient_balance,
                 "Insufficient Balance: ${balance}, unable to transfer '${total_transfer}' from account '${a}' to '${t}'", 
                 ("a",from_account.name)("t",to_account.name)("total_transfer",d.to_pretty_string(op.amount))("balance",d.to_pretty_string(d.get_balance(from_account, asset_type))) );

      return void_result();
   } FC_RETHROW_EXCEPTIONS( error, "Unable to transfer ${a} from ${f} to ${t}", ("a",d.to_pretty_string(op.amount))("f",op.from(d).name)("t",op.to(d).name) );

}  FC_CAPTURE_AND_RETHROW( (op) ) }

void_result transfer_evaluator::do_apply( const transfer_operation& o )
{ try {
   db().adjust_balance( o.from, -o.amount );
   db().adjust_balance( o.to, o.amount );
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result transfer_v2_evaluator::do_evaluate( const transfer_v2_operation& op )
{ try {

   const database& d = db();

   const account_object& from_account    = op.from(d);
   const account_object& to_account      = op.to(d);
   const asset_object&   asset_type      = op.amount.asset_id(d);
   const asset_object&   fee_asset_type  = op.fee.asset_id(d);

   try {

      // BSIP10 hard fork check
      if( d.head_block_time() <= HARDFORK_BSIP10_TIME )
         FC_THROW( "Operation requires hard fork BSIP10" );

      share_type required_core_fee = d.current_fee_schedule().calculate_fee( op, asset_type ).amount;
      GRAPHENE_ASSERT( core_fee_paid + max_fees_payable_with_coin_seconds >= required_core_fee,
                    insufficient_fee,
                    "Insufficient Fee Paid",
                    ("core_fee_paid",core_fee_paid)("required",required_core_fee) );
      // if some fees are paid with coin seconds
      if( core_fee_paid < required_core_fee )
         fees_paid_with_coin_seconds = required_core_fee - core_fee_paid;

      if( asset_type.options.flags & white_list )
      {
         GRAPHENE_ASSERT(
            from_account.is_authorized_asset( asset_type, d ),
            transfer_from_account_not_whitelisted,
            "'from' account ${from} is not whitelisted for asset ${asset}",
            ("from",op.from)
            ("asset",op.amount.asset_id)
            );
         GRAPHENE_ASSERT(
            to_account.is_authorized_asset( asset_type, d ),
            transfer_to_account_not_whitelisted,
            "'to' account ${to} is not whitelisted for asset ${asset}",
            ("to",op.to)
            ("asset",op.amount.asset_id)
            );
      }

      if( d.head_block_time() <= HARDFORK_419_TIME )
      {
         if( fee_asset_type.options.flags & white_list )
            FC_ASSERT( from_account.is_authorized_asset( asset_type, d ) );
      }
      // the above becomes no-op after hardfork because this check will then be performed in evaluator

      if( asset_type.is_transfer_restricted() )
      {
         GRAPHENE_ASSERT(
            from_account.id == asset_type.issuer || to_account.id == asset_type.issuer,
            transfer_restricted_transfer_asset,
            "Asset {asset} has transfer_restricted flag enabled",
            ("asset", op.amount.asset_id)
          );
      }

      bool insufficient_balance = d.get_balance( from_account, asset_type ).amount >= op.amount.amount;
      FC_ASSERT( insufficient_balance,
                 "Insufficient Balance: ${balance}, unable to transfer '${total_transfer}' from account '${a}' to '${t}'", 
                 ("a",from_account.name)("t",to_account.name)("total_transfer",d.to_pretty_string(op.amount))("balance",d.to_pretty_string(d.get_balance(from_account, asset_type))) );

      return void_result();
   } FC_RETHROW_EXCEPTIONS( error, "Unable to transfer ${a} from ${f} to ${t}", ("a",d.to_pretty_string(op.amount))("f",op.from(d).name)("t",op.to(d).name) );

}  FC_CAPTURE_AND_RETHROW( (op) ) }

void_result transfer_v2_evaluator::do_apply( const transfer_v2_operation& o )
{ try {
   pay_fee( o );
   db().adjust_balance( o.from, -o.amount );
   db().adjust_balance( o.to, o.amount );
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }


void transfer_v2_evaluator::pay_fee( const transfer_v2_operation& o )
{ try {
   database& d = db();
   const asset_object&   asset_type      = o.amount.asset_id(d);
   d.modify(*fee_paying_account_statistics, [&](account_statistics_object& s)
   {
      auto vesting_threshold = d.get_global_properties().parameters.cashback_vesting_threshold;
      auto fee_mode = asset_type.get_transfer_fee_mode();
      if( fee_mode == asset_transfer_fee_mode_flat )
      {
         s.pay_fee( core_fee_paid, vesting_threshold );
      }
      else if( fee_mode == asset_transfer_fee_mode_percentage_simple )
      {
         const auto& params = d.current_fee_schedule().find_op_fee_parameters( o );
         const auto& param = params.get<transfer_v2_operation::fee_parameters_type>();
         auto scaled_min_fee = fc::uint128( param.percentage_min_fee );
         scaled_min_fee *= d.current_fee_schedule().scale;
         scaled_min_fee /= GRAPHENE_100_PERCENT;
         s.pay_fee_pre_split_network( core_fee_paid, vesting_threshold, scaled_min_fee.to_uint64() );
      }
   });
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result override_transfer_evaluator::do_evaluate( const override_transfer_operation& op )
{ try {
   const database& d = db();

   const asset_object&   asset_type      = op.amount.asset_id(d);
   GRAPHENE_ASSERT(
      asset_type.can_override(),
      override_transfer_not_permitted,
      "override_transfer not permitted for asset ${asset}",
      ("asset", op.amount.asset_id)
      );
   FC_ASSERT( asset_type.issuer == op.issuer );

   const account_object& from_account    = op.from(d);
   const account_object& to_account      = op.to(d);
   const asset_object&   fee_asset_type  = op.fee.asset_id(d);

   if( asset_type.options.flags & white_list )
   {
      FC_ASSERT( to_account.is_authorized_asset( asset_type, d ) );
      FC_ASSERT( from_account.is_authorized_asset( asset_type, d ) );
   }

   if( d.head_block_time() <= HARDFORK_419_TIME )
   {
      if( fee_asset_type.options.flags & white_list )
         FC_ASSERT( from_account.is_authorized_asset( asset_type, d ) );
   }
   // the above becomes no-op after hardfork because this check will then be performed in evaluator

   FC_ASSERT( d.get_balance( from_account, asset_type ).amount >= op.amount.amount,
              "", ("total_transfer",op.amount)("balance",d.get_balance(from_account, asset_type).amount) );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result override_transfer_evaluator::do_apply( const override_transfer_operation& o )
{ try {
   db().adjust_balance( o.from, -o.amount );
   db().adjust_balance( o.to, o.amount );
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

} } // graphene::chain
