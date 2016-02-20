#include <graphene/chain/dividend_operation.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/dividend_operation_evaluator.hpp>
namespace graphene {
	namespace chain {
		share_type dividend_operation::calculate_fee(const fee_parameters_type& schedule)const
		{
			FC_ASSERT(holder_amount > 0);
			FC_ASSERT(holder_amount <= schedule.limited_shareholder,"amount of shareholder is out of limited");
			if (!schedule.if_native)
			{
				FC_ASSERT(schedule.if_active, "this operation inactive");
			}
			share_type core_fee_required = schedule.fee;
			if (if_show){
				core_fee_required += holder_amount*schedule.fee_per_shareholder;
			}
			else
			{
				core_fee_required += holder_amount*schedule.fee_per_shareholder;
			}
			if (describtion != "")
				core_fee_required += calculate_data_fee(fc::raw::pack_size(describtion), schedule.price_per_kbyte);
			return core_fee_required;
			
		}
		void dividend_operation::validate()const
		{	
			FC_ASSERT(fee.amount >= 0);
			FC_ASSERT(min_shares >= 1);
			FC_ASSERT(holder_amount > 0);
		}
		vector<pair<account_id_type, share_type>>dividend_operation::get_balance()const{
			dividend_operation_evaluator tmp_dp;
			return tmp_dp.get_balance(*this);
		}
		share_type dividend_operation_v2::calculate_fee(const fee_parameters_type& schedule)const
		{
			FC_ASSERT(holder_amount > 0);
			FC_ASSERT(holder_amount <= schedule.limited_shareholder, "amount of shareholder is out of limited");
			if (!schedule.if_native)
			{
				FC_ASSERT(schedule.if_active, "this operation inactive");
			}
			share_type core_fee_required = schedule.fee;
			if (if_show){
				core_fee_required += holder_amount*schedule.fee_per_shareholder;
			}
			else
			{
				core_fee_required += holder_amount*schedule.fee_per_shareholder;
			}
			if (describtion != "")
				core_fee_required += calculate_data_fee(fc::raw::pack_size(describtion), schedule.price_per_kbyte);
			return core_fee_required;

		}
		void dividend_operation_v2::validate()const
		{
			FC_ASSERT(fee.amount >= 0);
			FC_ASSERT(min_shares >= 1);
			FC_ASSERT(holder_amount > 0);
			FC_ASSERT(receivers.size());
		}
	}
}