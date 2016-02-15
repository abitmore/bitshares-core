#include <graphene/chain/protocol/dividend_operation.hpp>

namespace graphene {
	namespace chain {
		share_type dividend_operation::calculate_fee(const fee_parameters_type& schedule)const
		{
			share_type core_fee_required = schedule.fee;
			if (describtion!="")
				core_fee_required += calculate_data_fee(fc::raw::pack_size(describtion), schedule.price_per_kbyte);
			return core_fee_required;
		}
		void dividend_operation::validate()const
		{
			FC_ASSERT(fee.amount >= 0);
			FC_ASSERT(min_shares >= 1);
		}
	}
}