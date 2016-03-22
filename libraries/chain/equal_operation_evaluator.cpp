#include <fc/smart_ref_impl.hpp>

#include <graphene/chain/equal_operation_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/internal_exceptions.hpp>

#include <algorithm>
namespace graphene {
	namespace chain {
		void_result equal_bit_object_create_evaluator::do_evaluate(const equal_bit_object_create& o){
			try{
				database& d = db();

			}FC_CAPTURE_AND_RETHROW((o))
		}
		object_id_type equal_bit_object_create_evaluator::do_apply(const equal_bit_object_create& o){
			try{
				database& d = db();
				const auto& new_object = db().create<equal_bit_object>([&](equal_bit_object& obj){
					obj.depth = o.depth;
					obj.discripiton = o.description;
					obj.false_disc = o.false_disc;
					obj.owner = o.issuer;
					obj.true_disc = o.true_disc;
				});
				return new_object.id;
			}FC_CAPTURE_AND_RETHROW((o))
		}
	}
}