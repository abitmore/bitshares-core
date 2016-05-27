#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/equal_object.hpp>
namespace graphene {
	namespace chain {
		class equal_bit_object_create_evaluator : public evaluator <equal_bit_object_create_evaluator> {
		public:
			typedef equal_bit_object_create operation_type;

			void_result do_evaluate(const equal_bit_object_create& o);
			object_id_type do_apply(const equal_bit_object_create& o);
		};

		class equal_bit_input_evaluator : public evaluator <equal_bit_input_evaluator> {
		public:
			typedef equal_bit_input operation_type;

			void_result do_evaluate(const equal_bit_input& o);
			object_id_type do_apply(const equal_bit_input& o);
		};

		class equal_bit_output_evaluator : public evaluator <equal_bit_output_evaluator> {
		public:
			typedef equal_bit_output operation_type;

			void_result do_evaluate(const equal_bit_output& o);
			object_id_type do_apply(const equal_bit_output& o);
		};
	}
}
