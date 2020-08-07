/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#include "IO_abstract_hierarchy_iterator.h"

#include "BKE_duplilist.h"

extern "C" {
#include <limits.h> /* For INT_MAX. */
}
#include <cstring>
#include <sstream>

namespace blender {
namespace io {

ObjectIdentifier::ObjectIdentifier(Object *object,
                                   Object *duplicated_by,
                                   const PersistentID &persistent_id)
    : object(object), duplicated_by(duplicated_by), persistent_id(persistent_id)
{
}

ObjectIdentifier::ObjectIdentifier(const ObjectIdentifier &other)
    : object(other.object), duplicated_by(other.duplicated_by), persistent_id(other.persistent_id)
{
}

ObjectIdentifier::~ObjectIdentifier()
{
}

ObjectIdentifier ObjectIdentifier::for_real_object(Object *object)
{
  return ObjectIdentifier(object, nullptr, PersistentID());
}

ObjectIdentifier ObjectIdentifier::for_hierarchy_context(const HierarchyContext *context)
{
  if (context == nullptr) {
    return for_graph_root();
  }
  if (context->duplicator != nullptr) {
    return ObjectIdentifier(context->object, context->duplicator, context->persistent_id);
  }
  return for_real_object(context->object);
}

ObjectIdentifier ObjectIdentifier::for_duplicated_object(const DupliObject *dupli_object,
                                                         Object *duplicated_by)
{
  return ObjectIdentifier(dupli_object->ob, duplicated_by, PersistentID(dupli_object));
}

ObjectIdentifier ObjectIdentifier::for_graph_root()
{
  return ObjectIdentifier(nullptr, nullptr, PersistentID());
}

bool ObjectIdentifier::is_root() const
{
  return object == nullptr;
}

bool operator<(const ObjectIdentifier &obj_ident_a, const ObjectIdentifier &obj_ident_b)
{
  if (obj_ident_a.object != obj_ident_b.object) {
    return obj_ident_a.object < obj_ident_b.object;
  }

  if (obj_ident_a.duplicated_by != obj_ident_b.duplicated_by) {
    return obj_ident_a.duplicated_by < obj_ident_b.duplicated_by;
  }

  if (obj_ident_a.duplicated_by == nullptr) {
    /* Both are real objects, no need to check the persistent ID. */
    return false;
  }

  /* Same object, both are duplicated, use the persistent IDs to determine order. */
  return obj_ident_a.persistent_id < obj_ident_b.persistent_id;
}

bool operator==(const ObjectIdentifier &obj_ident_a, const ObjectIdentifier &obj_ident_b)
{
  if (obj_ident_a.object != obj_ident_b.object) {
    return false;
  }
  if (obj_ident_a.duplicated_by != obj_ident_b.duplicated_by) {
    return false;
  }
  if (obj_ident_a.duplicated_by == nullptr) {
    return true;
  }

  /* Same object, both are duplicated, use the persistent IDs to determine equality. */
  return obj_ident_a.persistent_id == obj_ident_b.persistent_id;
}

}  // namespace io
}  // namespace blender
