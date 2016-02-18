#include <graphene/chain/dividend_operation_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
namespace graphene {namespace chain {
void_result dividend_operation_evaluator::do_evaluate(const dividend_operation& op)
	{
		try{
				const database& d = db();
				const account_object& from_account = op.isser(d);
				const asset_id_type   share = op.shares_asset;
				const asset_id_type   dividend = op.dividend_asset;
				const asset_object&   asset_type_shares = op.shares_asset(d);
				const asset_object&   fee_asset_type = op.fee.asset_id(d);
				const asset_object&   asset_type_dividend = op.dividend_asset(d);
				const share_type min_shares = op.min_shares;
				const share_type value_per_shares = op.value_per_shares;
				const uint64_t holder_amount = op.holder_amount;
				
				FC_ASSERT(!asset_type_dividend.is_transfer_restricted()
					, "should restrict asset first");

				auto accounts_shares = d.get_balance(share, op.min_shares);
				//help to check if pay enough fee
				FC_ASSERT(holder_amount == accounts_shares.size());
				//calulate dividends
				dividends = 0;
				pair<account_id_type, asset> _account_dividends;
				accounts_dividends.reserve(accounts_shares.size());
				for (auto itr = accounts_shares.begin(); itr != accounts_shares.end(); itr++)
				{
					_account_dividends.first = itr->first;
					_account_dividends.second.asset_id = dividend;
					_account_dividends.second.amount= share_type(itr->second / min_shares*value_per_shares);
					accounts_dividends.push_back(_account_dividends);
					dividends = dividends +_account_dividends.second.amount;

				}
				bool insufficient_balance = d.get_balance(from_account, asset_type_dividend).amount >= dividends;
				FC_ASSERT(insufficient_balance,
					"Insufficient Balance: ${balance}, no enough shares to dividens '"
					);

				return void_result();

			}
		FC_CAPTURE_AND_RETHROW((op))
	}
void_result dividend_operation_evaluator::do_apply(const dividend_operation& op)
{try{
	ilog("start dividend");
	database& d = db();
	db().adjust_balance(op.isser, -asset(dividends, op.dividend_asset));
	for (auto itr = accounts_dividends.begin(); itr != accounts_dividends.end(); itr++)
	{
		d.adjust_balance(itr->first, itr->second);
	}
	ilog("finish dividend");
	return void_result();
	}
		
	FC_CAPTURE_AND_RETHROW((op))
	}
uint64_t dividend_operation_evaluator::get_asset_holder(const dividend_operation& op){
	const database& d = db();
	return d.get_satisfied_holder(op.shares_asset, op.min_shares);
}
vector<pair<account_id_type, share_type>> dividend_operation_evaluator::get_balance(const dividend_operation& op){
	const database& d = db();
	return d.get_balance(op.shares_asset, op.min_shares);
}
}}