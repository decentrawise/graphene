#pragma once
#include <boost/multiprecision/integer.hpp>
#include <graphene/protocol/object_id.hpp>
#include <fc/io/raw.hpp>
#include <fc/crypto/city.hpp>

#define MAX_NESTING (200)

namespace graphene { namespace db {
   /**
    *  @brief base for all database objects
    *
    *  The object is the fundamental building block of the database and
    *  is the level upon which undo/redo operations are performed.  Objects
    *  are used to track data and their relationships and provide an effecient
    *  means to find and update information.
    *
    *  Objects are assigned a unique and sequential object ID by the database within
    *  the id_space defined in the object.
    *
    *  All objects must be serializable via FC_REFLECT() and their content must be
    *  faithfully restored.   Additionally all objects must be copy-constructable and
    *  assignable in a relatively efficient manner.  In general this means that objects
    *  should only refer to other objects by ID and avoid expensive operations when
    *  they are copied, especially if they are modified frequently.
    *
    *  Additionally all objects may be annotated by plugins which wish to maintain
    *  additional information to an object.  There can be at most one annotation
    *  per id_space for each object.   An example of an annotation would be tracking
    *  extra data not required by validation such as the name and description of
    *  a user asset.  By carefully organizing how information is organized and
    *  tracked systems can minimize the workload to only that which is necessary
    *  to perform their function.
    *
    *  @note Do not use multiple inheritance with object because the code assumes
    *  a static_cast will work between object and derived types.
    */
   class object
   {
      public:
         object() = default;
         object( uint8_t space_id, uint8_t type_id ) : id( space_id, type_id, 0 ) {}
         virtual ~object() = default;

         // serialized
         object_id_type          id;

         /// these methods are implemented for derived classes by inheriting base_abstract_object<DerivedClass>
         /// @{
         virtual std::unique_ptr<object> clone()const = 0;
         virtual void                    move_from( object& obj ) = 0;
         virtual fc::variant             to_variant()const  = 0;
         virtual std::vector<char>       pack()const = 0;
         /// @}
   };

   /**
    * @class base_abstract_object
    * @brief   Use the Curiously Recurring Template Pattern to automatically add the ability to
    *  clone, serialize, and move objects polymorphically.
    *
    *  http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
    */
   template<typename DerivedClass>
   class base_abstract_object : public object
   {
      public:
         using object::object; // constructors
         std::unique_ptr<object> clone()const override
         {
            return std::make_unique<DerivedClass>( *static_cast<const DerivedClass*>(this) );
         }

         void    move_from( object& obj ) override
         {
            static_cast<DerivedClass&>(*this) = std::move( static_cast<DerivedClass&>(obj) );
         }
         fc::variant to_variant()const override
         { return fc::variant( static_cast<const DerivedClass&>(*this), MAX_NESTING ); }
         std::vector<char> pack()const override { return fc::raw::pack( static_cast<const DerivedClass&>(*this) ); }
   };

   template<typename DerivedClass, uint8_t SpaceID, uint8_t TypeID>
   class abstract_object : public base_abstract_object<DerivedClass>
   {
   public:
      static constexpr uint8_t space_id = SpaceID;
      static constexpr uint8_t type_id = TypeID;
      abstract_object() : base_abstract_object<DerivedClass>( space_id, type_id ) {}
      object_id<SpaceID,TypeID> get_id() const { return object_id<SpaceID,TypeID>( this->id ); }
   };

   using annotation_map = fc::flat_map<uint8_t, object_id_type>;

   /**
    *  @class annotated_object
    *  @brief An object that is easily extended by providing pointers to other objects, one for each space.
    */
   template<typename DerivedClass>
   class annotated_object : public base_abstract_object<DerivedClass>
   {
      public:
         /** return object_id_type() if no anotation is found for id_space */
         object_id_type          get_annotation( uint8_t annotation_id_space )const
         {
            auto itr = annotations.find(annotation_id_space);
            if( itr != annotations.end() ) return itr->second;
            return object_id_type();
         }
         void                    set_annotation( object_id_type id )
         {
            annotations[id.space()] = id;
         }

         /**
          *  Annotations should be accessed via get_annotation and set_annotation so
          *  that they can be maintained in sorted order.
          */
         annotation_map annotations;
   };

} } // graphene::db

// Without this, pack(object_id) tries to match the template for
// pack(boost::multiprecision::uint128_t). No idea why. :-(
namespace boost { namespace multiprecision { namespace detail {
template<typename To>
struct is_restricted_conversion<graphene::db::object,To> : public mpl::true_ {};
}}}

FC_REFLECT_TYPENAME( graphene::db::annotation_map )
FC_REFLECT( graphene::db::object, (id) )
FC_REFLECT_DERIVED_TEMPLATE( (typename Derived), graphene::db::annotated_object<Derived>, (graphene::db::object),
                             (annotations) )
