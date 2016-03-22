#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/memo.hpp>

namespace graphene {
	namespace chain {
		struct equal_obeject_create : public base_operation
		{
			struct fee_parameters_type {
				uint64_t fee = 200 * GRAPHENE_BLOCKCHAIN_PRECISION;
				uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION; /// only required for large memos.
				bool if_native = false;
				bool if_active = true;
			};

			account_id_type issuer;
			/** if public, if it is private ,only the owner can create operation on this policy
			*/
			bool public_policy;

			uint8_t depth;

			string discripiton;

			string true_disc;

			string false_disc;

			account_id_type fee_payer()const { return issuer; }
			void            validate()const;
			share_type      calculate_fee(const fee_parameters_type& k)const;
			
		};
	}
}