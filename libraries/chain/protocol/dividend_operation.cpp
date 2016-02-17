#include <graphene/chain/dividend_operation.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/dividend_operation_evaluator.hpp>
namespace graphene {
	namespace chain {
		share_type dividend_operation::calculate_fee(const fee_parameters_type& schedule)const
		{
			dividend_operation_evaluator tmp_dp;
			
			share_type core_fee_required = schedule.fee;
			if (if_show){
				core_fee_required += tmp_dp.get_asset_holder(*this)*schedule.fee_per_shareholder;
			}
			else
			{
				core_fee_required += tmp_dp.get_asset_holder(*this)*schedule.fee_per_shareholder;
			}
			if (describtion != "")
				core_fee_required += calculate_data_fee(fc::raw::pack_size(describtion), schedule.price_per_kbyte);
				return core_fee_required;
			
		}
		void dividend_operation::validate()const
		{
			FC_ASSERT(fee.amount >= 0);
			FC_ASSERT(min_shares >= 1);
		}
		vector<pair<account_id_type, share_type>>dividend_operation::get_balance(){
			dividend_operation_evaluator tmp_dp;
			return tmp_dp.get_balance(*this);
		}
	}
}