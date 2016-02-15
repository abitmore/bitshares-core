#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/dividend_operation.hpp>

namespace graphene {
	namespace chain {

		class dividend_operation_evaluator : public evaluator <dividend_operation_evaluator>
		{
		public:
			typedef dividend_operation operation_type;
			share_type dividends;
			vector<pair<account_id_type, asset>> accounts_dividends;
			void_result do_evaluate(const dividend_operation& op);
			void_result do_apply(const dividend_operation& op);
		};


	}
} // graphene::chain