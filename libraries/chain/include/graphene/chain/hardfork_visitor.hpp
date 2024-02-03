#pragma once

#include <graphene/protocol/operations.hpp>

#include <graphene/chain/hardfork.hpp>

#include <fc/reflect/typelist.hpp>

#include <type_traits>
#include <functional>

namespace graphene { namespace chain {

/**
 * @brief The hardfork_visitor struct checks whether a given operation type has been hardforked in or not
 *
 * This visitor can be invoked in several different ways, including operation::visit, typelist::runtime::dispatch, or
 * direct invocation by calling the visit() method passing an operation variant, narrow operation type, operation tag,
 * or templating on the narrow operation type
 */
struct hardfork_visitor {
   using result_type = bool;
   using last_base_op = protocol::htlc_refund_operation;
   // using hfXXXX_ops = fc::typelist::list< protocol::first_operation,
   //                                        protocol::other_operation>;

   fc::time_point_sec now;

   /// @note using head block time for all operations
   explicit hardfork_visitor(const fc::time_point_sec& head_block_time) : now(head_block_time) {}

   /// The real visitor implementations. Future operation types get added in here.
   /// @{
   template<typename Op>
   std::enable_if_t<operation::tag<Op>::value <= protocol::operation::tag<last_base_op>::value, bool>
   visit() { return true; }
   // template<typename Op>
   // std::enable_if_t<fc::typelist::contains<hfXXXX_ops, Op>(), bool>
   // visit() { return HARDFORK_XXXX_PASSED(now); }
   /// @}

   /// typelist::runtime::dispatch adaptor
   template<class W, class Op=typename W::type>
   std::enable_if_t<fc::typelist::contains<protocol::operation::list, Op>(), bool>
   operator()(W) { return visit<Op>(); }
   /// static_variant::visit adaptor
   template<class Op>
   std::enable_if_t<fc::typelist::contains<protocol::operation::list, Op>(), bool>
   operator()(const Op&) { return visit<Op>(); }
   /// Tag adaptor
   bool visit(protocol::operation::tag_type tag) const {
      return fc::typelist::runtime::dispatch(protocol::operation::list(), (size_t)tag, *this);
   }
   /// operation adaptor
   bool visit(const protocol::operation& op) const {
      return visit(op.which());
   }
};

} } // namespace graphene::chain
