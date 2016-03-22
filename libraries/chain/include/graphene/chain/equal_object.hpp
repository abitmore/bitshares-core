#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace graphene {
	namespace chain {
		class database;
		/**
		* @class equial_bit_object
		* @ingroup object
		* @ingroup implementation
		*
		* This object contains is to describe a policy that compare two input if equal
		*/
		class equal_bit_object : public graphene::db::abstract_object < equal_bit_object >
		{
		public:
			static const uint8_t space_id = implementation_ids;
			static const uint8_t type_id = policy_equal_bit_object_type;
			/** the people who owner this policy ,he can choice if it is a public policy 
			*/
			account_id_type owner;
			/** if public, if it is private ,only the owner can create operation on this policy
			*/
			bool public_policy;

			uint8_t depth;

			string discripiton;

			string true_disc;

			string false_disc;

			operation_history_id_type  unreciver_policy;

		};
		struct by_owner{};

		/**
		* @ingroup object_index
		*/
		typedef multi_index_container<
			equal_bit_object,
			indexed_by<
			ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
			ordered_unique< tag<by_owner>, member<equal_bit_object, account_id_type, &equal_bit_object::owner> >
			>
		> equal_bit_multi_index_type;

		/**
		* @ingroup object_index
		*/
		typedef generic_index<equal_bit_object, equal_bit_multi_index_type> account_bit_index;
	}
}
