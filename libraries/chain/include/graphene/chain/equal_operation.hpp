#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/memo.hpp>

namespace graphene {
	namespace chain {
		struct equal_bit_obeject_create : public base_operation
		{
			struct fee_parameters_type {
				uint64_t fee = 2000 * GRAPHENE_BLOCKCHAIN_PRECISION;
				uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION; /// only required for large memos.
				bool if_native = false;
				bool if_active = true;
			};

			account_id_type issuer;
			/** if public, if it is private ,only the owner can create operation on this policy
			*/
			bool public_policy;

			uint8_t depth;

			string description;

			string true_disc;

			string false_disc;

			account_id_type fee_payer()const { return issuer; }
			void            validate()const;
			share_type      calculate_fee(const fee_parameters_type& k)const;
			
		};
		struct equal_bit_input : public base_operation
		{
			struct fee_parameters_type {
				uint64_t fee = 2 * GRAPHENE_BLOCKCHAIN_PRECISION;
				uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION; /// only required for large memos.
			};
			account_id_type issuer;
			equal_bit_id_type equal_id;
			uint8_t depth;
			asset input_amount;
			uint16_t output_ratio;
			vector<bool> input_value;
			account_id_type fee_payer()const { return issuer; }
			void            validate()const;
			share_type      calculate_fee(const fee_parameters_type& k)const;
		};
		struct equal_bit_output : public base_operation
		{
			struct fee_parameters_type {
				uint64_t fee = 2 * GRAPHENE_BLOCKCHAIN_PRECISION;
				uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION; /// only required for large memos.
			};
			account_id_type issuer;
			equal_bit_id_type equal_id;
			operation_history_id_type input_operation_id;
			vector<bool> input_value;
			account_id_type fee_payer()const { return issuer; }
			void            validate()const;
			share_type      calculate_fee(const fee_parameters_type& k)const;
		};
	}
}