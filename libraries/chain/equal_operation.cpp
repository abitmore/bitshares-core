#include <graphene/chain/database.hpp>
#include <graphene\chain\equal_operation.hpp>
namespace graphene {
	namespace chain {
		void equal_bit_object_create::validate()const{
			FC_ASSERT(depth >= 1 && depth <= 16);
		}
		share_type equal_bit_object_create::calculate_fee(const fee_parameters_type& schedule)const{
			if (!schedule.if_native)
			{
				FC_ASSERT(schedule.if_active, "this operation inactive");
			};
			share_type core_fee_required = schedule.fee;
			if (description != "")
				core_fee_required += calculate_data_fee(fc::raw::pack_size(description), schedule.price_per_kbyte);
			return core_fee_required;
		}
		void equal_bit_input::validate()const{
			FC_ASSERT(depth >= 1 && depth <= 16);
			FC_ASSERT(input_amount.amount > 0);
			FC_ASSERT(output_ratio > 0);
		}
		share_type equal_bit_input::calculate_fee(const fee_parameters_type& schedule)const{
			return schedule.fee;
		}
		void equal_bit_output::validate()const{

		}
		share_type equal_bit_output::calculate_fee(const fee_parameters_type& schedule)const{
			return schedule.fee;
		}
	}
}