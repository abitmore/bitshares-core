#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene {
	namespace chain {
		class dividend_operation_evaluator : public evaluator < dividend_operation_evaluator >
		{
		public:
			typedef dividend_operation operation_type;
			// total dividends to dividend
			share_type dividends;
			// accounts those dividend to
			vector<pair<account_id_type, asset>> accounts_dividends;
			void_result do_evaluate(const dividend_operation& op);
			void_result do_apply(const dividend_operation& op);
			// help to calculate fee
			uint64_t get_asset_holder(const dividend_operation& op);
			vector<pair<account_id_type, share_type>> get_balance(const dividend_operation& op);
		};
		class dividend_operation_v2_evaluator : public evaluator < dividend_operation_evaluator >
		{
		public:
			typedef dividend_operation_v2 operation_type;
			// total dividends to dividend
			share_type dividends;
			void_result do_evaluate(const dividend_operation_v2& op);
			void_result do_apply(const dividend_operation_v2& op);
			// help to calculate fee
		};
	}} // graphene::chain