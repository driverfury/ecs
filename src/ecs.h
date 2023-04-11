#ifndef ECS_H
#define ECS_H

size_t  ecs_world_create(void);
void    ecs_world_destroy(size_t world_id);
void    ecs_world_current_set(size_t world_id);
size_t  ecs_world_current_get(void);

size_t  ecs_entity_create(void);
void    ecs_entity_destroy(size_t entity_id);

size_t  ecs_component_register(size_t component_size);
void    ecs_component_unregister(size_t component_id);

void    ecs_entity_component_attach(size_t entity_id, size_t component_id);
void    ecs_entity_component_detach(size_t entity_id, size_t component_id);

void   *ecs_entity_component_get(size_t entity_id, size_t component_id);
void    ecs_update(void);

typedef struct
ecs_query_result
{
    size_t count;
    void ***list;
} ecs_query_result;

ecs_query_result *ecs_query(size_t num_components, ...);

#endif

#ifdef ECS_IMPLEMENTATION
#ifndef ECS_IMPLEMENTED
#define ECS_IMPLEMENTED

#include <stddef.h>

/* Utils */

#ifndef ecs_malloc
#include <stdlib.h>
#define ecs_malloc malloc
#define ecs_realloc realloc
#define ecs_free free
#endif

#define da_malloc ecs_malloc
#define da_realloc ecs_realloc
#define da_free ecs_free
#include "darray.h"

void
ecs_mem_copy(void *src, void *dst, size_t num_bytes)
{
    size_t i;
    unsigned char *s, *d;

    s = (unsigned char *)src;
    d = (unsigned char *)dst;
    for(i = 0;
        i < num_bytes;
        ++i)
    {
        *d++ = *s++;
    }
}

void
ecs_mem_zero(void *dst, size_t num_bytes)
{
    size_t i;
    unsigned char *d;

    d = (unsigned char *)dst;
    for(i = 0;
        i < num_bytes;
        ++i)
    {
        *d++ = 0;
    }
}

/* Map */

typedef struct
ecs_map_entry
{
    size_t key;
    size_t value;
    int free;
} ecs_map_entry;

typedef struct
ecs_map
{
    ecs_map_entry *entries;
} ecs_map;

size_t*
ecs_map_get(ecs_map *map, size_t key)
{
    size_t i;

    for(i = 0;
        i < da_len(map->entries);
        ++i)
    {
        if(map->entries[i].free)
        {
            continue;
        }

        if(map->entries[i].key == key)
        {
            return(&(map->entries[i].value));
        }
    }

    return(0);
}

void
ecs_map_add(ecs_map *map, size_t key, size_t value)
{
    ecs_map_entry *free_entry;
    size_t i;

    free_entry = 0;
    for(i = 0;
        i < da_len(map->entries);
        ++i)
    {
        if(map->entries[i].free)
        {
            free_entry = &(map->entries[i]);
            break;
        }
    }

    if(free_entry)
    {
        free_entry->free = 0;
        free_entry->key = key;
        free_entry->value = value;
    }
    else
    {
        ecs_map_entry entry = {0};
        entry.key = key;
        entry.value = value;
        entry.free = 0;
        da_push(map->entries, entry);
    }
}

void
ecs_map_set(ecs_map *map, size_t key, size_t value)
{
    size_t *value_ptr;

    value_ptr = ecs_map_get(map, key);
    if(value_ptr)
    {
        *value_ptr = value;
    }
    else
    {
        ecs_map_add(map, key, value);
    }
}

void
ecs_map_unset(ecs_map *map, size_t key)
{
    size_t i;

    for(i = 0;
        i < da_len(map->entries);
        ++i)
    {
        if(map->entries[i].free)
        {
            continue;
        }

        if(map->entries[i].key == key)
        {
            map->entries[i].free = 1;
            break;
        }
    }
}

void
ecs_map_free(ecs_map *map)
{
    da_free(map->entries);
}

/* Entity manager */

#define ECS_COMPONENT_MASK_BITS (sizeof(size_t)*8)

typedef struct
ecs_entity
{
    size_t id;
    size_t *component_mask;
    int dead;
    int destroyed;
} ecs_entity;

typedef struct
ecs_entity_manager
{
    size_t current_id;

    size_t cap;
    ecs_entity *entities;

    ecs_map id_to_index;
    ecs_map index_to_id;

    size_t *free_slots;
} ecs_entity_manager;

size_t
ecs_entity_manager_create(ecs_entity_manager *entity_manager)
{
    size_t entity_id, entity_index;
    ecs_entity entity = {0};
    size_t free_slots_length;

    entity_id = ++entity_manager->current_id;
    entity.id = entity_id;

    /* TODO: Check index in free_slots first */
    free_slots_length = da_len(entity_manager->free_slots);
    if(free_slots_length > 0)
    {
        entity_index = entity_manager->free_slots[free_slots_length - 1];
        da_pop(entity_manager->free_slots);
        entity_manager->entities[entity_index] = entity;
    }
    else
    {
        entity_index = da_len(entity_manager->entities);
        da_push(entity_manager->entities, entity);
        entity_manager->cap += 1;
    }

    ecs_map_set(&entity_manager->id_to_index, entity_id, entity_index);
    ecs_map_set(&entity_manager->index_to_id, entity_index, entity_id);

    return(entity_id);
}

void
ecs_entity_manager_destroy(
    ecs_entity_manager *entity_manager,
    size_t entity_id)
{
    size_t *entity_index_ptr, entity_index;
    ecs_entity *entity;

    entity_index_ptr = ecs_map_get(&entity_manager->id_to_index, entity_id);
    if(!entity_index_ptr)
    {
        return;
    }

    entity_index = *entity_index_ptr;
    entity = &(entity_manager->entities[entity_index]);

    da_push(entity_manager->free_slots, entity_index);
    ecs_map_unset(&entity_manager->id_to_index, entity_id);
    ecs_map_unset(&entity_manager->index_to_id, entity_index);
    entity->destroyed = 1;
}

ecs_entity*
ecs_entity_manager_get_at(
    ecs_entity_manager *entity_manager,
    size_t index)
{
    if(index >= da_len(entity_manager->entities))
    {
        return(0);
    }

    return(&(entity_manager->entities[index]));
}

ecs_entity*
ecs_entity_manager_get(
    ecs_entity_manager *entity_manager,
    size_t entity_id)
{
    size_t *entity_index_ptr, entity_index;

    entity_index_ptr = ecs_map_get(&entity_manager->id_to_index, entity_id);
    if(!entity_index_ptr)
    {
        return(0);
    }

    entity_index = *entity_index_ptr;
    return(ecs_entity_manager_get_at(entity_manager, entity_index));
}

/* Component list */

typedef struct
ecs_component_list
{
    size_t id;

    ecs_map entity_to_index;
    ecs_map index_to_entity;

    int destroyed;

    size_t unit_size;
    size_t count;
    size_t cap;
    void *data;
} ecs_component_list;

void*
ecs_component_list_get_at(
    ecs_component_list *component_list,
    size_t index)
{
    if(index >= component_list->count)
    {
        return(0);
    }

    return((void *)((unsigned char *)component_list->data + index*component_list->unit_size));
}

void*
ecs_component_list_get(
    ecs_component_list *component_list,
    size_t entity_id)
{
    size_t *index;

    index = ecs_map_get(&component_list->entity_to_index, entity_id);
    if(!index)
    {
        return(0);
    }

    return(ecs_component_list_get_at(component_list, *index));
}

void
ecs_component_list_add(
    ecs_component_list *component_list,
    size_t entity_id,
    void *component)
{
    size_t index;
    void *src, *dst;

    if(ecs_map_get(&component_list->entity_to_index, entity_id))
    {
        /* Component already assigned to this entity */
        return;
    }

    index = component_list->count;

    if(component_list->cap == 0)
    {
        component_list->cap = 1;
        component_list->data = ecs_malloc(component_list->cap*component_list->unit_size);
        if(!component_list->data)
        {
            component_list->cap = 0;
            return;
        }
    }
    else
    {
        if(component_list->cap <= component_list->count)
        {
            component_list->cap = component_list->cap*2 + 1;
            component_list->data = ecs_realloc(component_list->data, component_list->cap*component_list->unit_size);
            if(!component_list->data)
            {
                component_list->cap = 0;
                return;
            }
        }
    }

    ecs_map_set(&component_list->entity_to_index, entity_id, index);
    ecs_map_set(&component_list->index_to_entity, index, entity_id);

    component_list->count += 1;

    dst = ecs_component_list_get_at(component_list, index);
    if(component)
    {
        src = (void *)((unsigned char *)component);
        ecs_mem_copy(src, dst, component_list->unit_size);
    }
    else
    {
        ecs_mem_zero(dst, component_list->unit_size);
    }
}

void
ecs_component_list_remove(
    ecs_component_list *component_list,
    size_t entity_id)
{
    size_t *index_ptr, index, last_index, *last_entity_ptr, last_entity;
    void *src, *dst;

    if(!component_list->count)
    {
        return;
    }

    index_ptr = ecs_map_get(&component_list->entity_to_index, entity_id);
    if(!index_ptr)
    {
        /* Entity does not have this component */
        return;
    }

    index = *index_ptr;
    last_index = component_list->count - 1;

    src = ecs_component_list_get_at(component_list, last_index);
    dst = ecs_component_list_get_at(component_list, index);
    ecs_mem_copy(src, dst, component_list->unit_size);

    last_entity_ptr = ecs_map_get(&component_list->index_to_entity, last_index);
    if(!last_entity_ptr)
    {
        /* TODO: DEBUG: Is this behavior correct? */
        return;
    }

    last_entity = *last_entity_ptr;
    ecs_map_set(&component_list->entity_to_index, last_entity, index);
    ecs_map_set(&component_list->index_to_entity, index, last_entity);

    ecs_map_unset(&component_list->entity_to_index, entity_id);
    ecs_map_unset(&component_list->index_to_entity, last_index);
}

void
ecs_component_list_entity_destroyed(
    ecs_component_list *component_list,
    size_t entity_id)
{
    ecs_component_list_remove(component_list, entity_id);
}

/* Component manager */

typedef struct
ecs_component_manager
{
    size_t current_id;
    ecs_component_list *lists;
    size_t cap;

    ecs_map id_to_index;
    ecs_map index_to_id;

    size_t *free_slots;
} ecs_component_manager;

size_t
ecs_component_manager_register(
    ecs_component_manager *component_manager,
    size_t component_size)
{
    size_t component_id, component_index;
    ecs_component_list list = {0};
    size_t free_slots_length;

    if(component_size == 0)
    {
        return(0);
    }

    /* TODO: Check index in free_slots first */
    component_id = ++component_manager->current_id;
    list.id = component_id;
    list.unit_size = component_size;

    free_slots_length = da_len(component_manager->free_slots);
    if(free_slots_length > 0)
    {
        component_index = component_manager->free_slots[free_slots_length - 1];
        da_pop(component_manager->free_slots);
        component_manager->lists[component_index] = list;
    }
    else
    {
        component_index = da_len(component_manager->lists);
        da_push(component_manager->lists, list);
        component_manager->cap += 1;
    }

    ecs_map_set(&component_manager->id_to_index, component_id, component_index);
    ecs_map_set(&component_manager->index_to_id, component_index, component_id);

    return(component_id);
}

void
ecs_component_manager_unregister(
    ecs_component_manager *component_manager,
    size_t component_id)
{
    size_t *component_index_ptr, component_index;
    ecs_component_list *list;

    component_index_ptr = ecs_map_get(&component_manager->id_to_index, component_id);
    if(!component_index_ptr)
    {
        return;
    }

    component_index = *component_index_ptr;
    list = &(component_manager->lists[component_index]);

    da_push(component_manager->free_slots, component_index);
    ecs_map_free(&list->entity_to_index);
    ecs_map_free(&list->index_to_entity);
    ecs_free(list->data);

    list->destroyed = 1;
}

void
ecs_component_manager_add(
    ecs_component_manager *component_manager,
    size_t entity_id,
    size_t component_id)
{
    size_t *list_index_ptr, list_index;
    ecs_component_list *list;

    list_index_ptr = ecs_map_get(&component_manager->id_to_index, component_id);
    if(!list_index_ptr)
    {
        return;
    }

    list_index = *list_index_ptr;
    list = &(component_manager->lists[list_index]);

    ecs_component_list_add(list, entity_id, 0);
}

void
ecs_component_manager_remove(
    ecs_component_manager *component_manager,
    size_t entity_id,
    size_t component_id)
{
    size_t *list_index_ptr, list_index;
    ecs_component_list *list;

    list_index_ptr = ecs_map_get(&component_manager->id_to_index, component_id);
    if(!list_index_ptr)
    {
        return;
    }

    list_index = *list_index_ptr;
    list = &(component_manager->lists[list_index]);

    ecs_component_list_remove(list, entity_id);
}

void*
ecs_component_manager_get(
    ecs_component_manager *component_manager,
    size_t entity_id,
    size_t component_id)
{
    size_t *list_index_ptr, list_index;
    ecs_component_list *list;

    list_index_ptr = ecs_map_get(&component_manager->id_to_index, component_id);
    if(!list_index_ptr)
    {
        return(0);
    }

    list_index = *list_index_ptr;
    list = &(component_manager->lists[list_index]);

    return(ecs_component_list_get(list, entity_id));
}

void
ecs_component_manager_entity_destroyed(
    ecs_component_manager *component_manager,
    size_t entity_id)
{
    size_t lists_count;
    size_t i;

    lists_count = da_len(component_manager->lists);
    for(i = 0;
        i < lists_count;
        ++i)
    {
        ecs_component_list_entity_destroyed(&(component_manager->lists[i]), entity_id);
    }
}

/* World */

typedef struct
ecs_world
{
    size_t id;

    ecs_entity_manager entity_manager;
    ecs_component_manager component_manager;
    ecs_query_result query_result;

    int dead;
    int destroyed;
} ecs_world;

size_t
ecs_world_entity_create(ecs_world *world)
{
    size_t entity_id;

    entity_id = ecs_entity_manager_create(&world->entity_manager);

    return(entity_id);
}

void
ecs_world_entity_destroy(ecs_world *world, size_t entity_id)
{
    ecs_entity_manager_destroy(&world->entity_manager, entity_id);
    ecs_component_manager_entity_destroyed(&world->component_manager, entity_id);
}

size_t
ecs_world_component_register(ecs_world *world, size_t component_size)
{
    size_t component_id;

    component_id = ecs_component_manager_register(&world->component_manager, component_size);

    return(component_id);
}

void
ecs_world_component_unregister(ecs_world *world, size_t component_id)
{
    ecs_component_manager_unregister(&world->component_manager, component_id);
}

void
ecs_world_entity_component_attach(
    ecs_world *world,
    size_t entity_id,
    size_t component_id)
{
    ecs_entity *entity;
    size_t mask_size;
    size_t mask_index;
    size_t mask_shift;

    entity = ecs_entity_manager_get(&world->entity_manager, entity_id);
    if(!entity)
    {
        return;
    }

    ecs_component_manager_add(&world->component_manager, entity_id, component_id);

    mask_size = da_len(entity->component_mask);
    mask_index = (component_id - 1) / ECS_COMPONENT_MASK_BITS;
    mask_shift = (component_id - 1) % ECS_COMPONENT_MASK_BITS;

    while(mask_size <= mask_index)
    {
        da_push(entity->component_mask, 0);
        mask_size += 1;
    }

    entity->component_mask[mask_index] |= ((size_t)1 << mask_shift);
}

void
ecs_world_entity_component_detach(
    ecs_world *world,
    size_t entity_id,
    size_t component_id)
{
    ecs_entity *entity;
    size_t mask_size;
    size_t mask_index;
    size_t mask_shift;

    entity = ecs_entity_manager_get(&world->entity_manager, entity_id);
    if(!entity)
    {
        return;
    }

    ecs_component_manager_remove(&world->component_manager, entity_id, component_id);

    mask_size = da_len(entity->component_mask);
    mask_index = (component_id - 1) / ECS_COMPONENT_MASK_BITS;
    mask_shift = (component_id - 1) % ECS_COMPONENT_MASK_BITS;

    while(mask_size <= mask_index)
    {
        da_push(entity->component_mask, 0);
        mask_size += 1;
    }

    entity->component_mask[mask_index] &= ~(1 << mask_shift);
}

int
ecs_world_entity_component_has(
    ecs_world *world,
    size_t entity_id,
    size_t component_id)
{
    ecs_entity *entity;
    size_t mask_size;
    size_t mask_index;
    size_t mask_shift;

    entity = ecs_entity_manager_get(&world->entity_manager, entity_id);
    if(!entity || entity->dead)
    {
        return(0);
    }

    mask_size = da_len(entity->component_mask);
    mask_index = (component_id - 1) / ECS_COMPONENT_MASK_BITS;
    mask_shift = (component_id - 1) % ECS_COMPONENT_MASK_BITS;

    if(mask_size <= mask_index)
    {
        return(0);
    }

    if((entity->component_mask[mask_index] & ((size_t)1 << mask_shift)) != 0)
    {
        return(1);
    }

    return(0);
}

void*
ecs_world_entity_component_get(
    ecs_world *world,
    size_t entity_id,
    size_t component_id)
{
    if(!ecs_world_entity_component_has(world, entity_id, component_id))
    {
        return(0);
    }

    return(ecs_component_manager_get(&world->component_manager, entity_id, component_id));
}

#include <stdarg.h>

ecs_query_result*
ecs_world_query(ecs_world *world, size_t num_components, va_list args)
{
    size_t entity_index;
    size_t *components_ids;
    size_t components_count;
    size_t *entities_ids;
    size_t entities_count;
    size_t i;
    ecs_query_result *result;

    result = &world->query_result;

    components_ids = 0;
    for(i = 0;
        i < num_components;
        ++i)
    {
        size_t component_id = va_arg(args, size_t);
        if(component_id == 0)
        {
            continue;
        }

        da_push(components_ids, component_id);
    }

    components_count = da_len(components_ids);

    entities_ids = 0;
    entities_count = 0;
    for(entity_index = 0;
        entity_index < world->entity_manager.cap;
        ++entity_index)
    {
        int has_required_components;
        size_t *entity_id_ptr, entity_id;

        entity_id_ptr = ecs_map_get(&world->entity_manager.index_to_id, entity_index);
        if(!entity_id_ptr)
        {
            continue;
        }

        entity_id = *entity_id_ptr;

        has_required_components = 1;
        for(i = 0;
            i < components_count;
            ++i)
        {
            if(!ecs_world_entity_component_has(world, entity_id, components_ids[i]))
            {
                has_required_components = 0;
                break;
            }
        }

        if(has_required_components)
        {
            da_push(entities_ids, entity_id);
        }
    }

    entities_count = da_len(entities_ids);

    while(da_len(result->list) < entities_count)
    {
        da_push(result->list, 0);
    }

    for(entity_index = 0;
        entity_index < entities_count;
        ++entity_index)
    {
        while(da_len(result->list[entity_index]) < components_count)
        {
            da_push(result->list[entity_index], 0);
        }

        for(i = 0;
            i < components_count;
            ++i)
        {
            result->list[entity_index][i] = ecs_world_entity_component_get(world, entities_ids[entity_index], components_ids[i]);
        }
    }

    result->count = entities_count;

    return(result);
}

typedef struct
ecs_world_manager
{
    ecs_map id_to_index;
    ecs_map index_to_id;

    ecs_world *worlds;
    size_t cap;
    size_t current_id;

    size_t *free_slots;
} ecs_world_manager;

size_t
ecs_world_manager_create(ecs_world_manager *world_manager)
{
    size_t world_id, world_index;
    ecs_world world = {0};
    size_t free_slots_length;

    /* TODO: Check index in free_slots first */
    world_id = ++world_manager->current_id;
    world.id = world_id;

    free_slots_length = da_len(world_manager->free_slots);
    if(free_slots_length > 0)
    {
        world_index = world_manager->free_slots[free_slots_length - 1];
        da_pop(world_manager->free_slots);
        world_manager->worlds[world_index] = world;
    }
    else
    {
        world_index = da_len(world_manager->worlds);
        da_push(world_manager->worlds, world);
        world_manager->cap += 1;
    }

    ecs_map_set(&world_manager->id_to_index, world_id, world_index);
    ecs_map_set(&world_manager->index_to_id, world_index, world_id);

    world_manager->cap += 1;

    return(world_id);
}

void
ecs_world_manager_destroy(
    ecs_world_manager *world_manager,
    size_t world_id)
{
    size_t *world_index_ptr, world_index;
    size_t entity_index, component_index;
    ecs_world *world;

    world_index_ptr = ecs_map_get(&world_manager->id_to_index, world_id);
    if(!world_index_ptr)
    {
        return;
    }

    world_index = *world_index_ptr;
    world = &(world_manager->worlds[world_index]);

    da_push(world_manager->free_slots, world_index);

    for(entity_index = 0;
        entity_index < world->entity_manager.cap;
        ++entity_index)
    {
        ecs_entity *entity;

        entity = &(world->entity_manager.entities[entity_index]);
        if(entity->destroyed)
        {
            continue;
        }

        ecs_entity_manager_destroy(&world->entity_manager, entity->id);
        ecs_component_manager_entity_destroyed(&world->component_manager, entity->id);
    }

    da_free(world->entity_manager.entities);
    da_free(world->entity_manager.free_slots);

    world->entity_manager.current_id = 0;
    world->entity_manager.cap = 0;
    world->entity_manager.entities = 0;
    world->entity_manager.free_slots = 0;

    ecs_map_free(&world->entity_manager.id_to_index);
    ecs_map_free(&world->entity_manager.index_to_id);

    for(component_index = 0;
        component_index < world->component_manager.cap;
        ++component_index)
    {
        ecs_component_list *list;

        list = &(world->component_manager.lists[component_index]);
        if(list->destroyed)
        {
            continue;
        }

        ecs_component_manager_unregister(&world->component_manager, list->id);
    }

    da_free(world->component_manager.lists);
    da_free(world->component_manager.free_slots);

    world->component_manager.current_id = 0;
    world->component_manager.cap = 0;
    world->component_manager.lists = 0;
    world->component_manager.free_slots = 0;

    ecs_map_free(&world->component_manager.id_to_index);
    ecs_map_free(&world->component_manager.index_to_id);

    da_free(world->query_result.list);
    world->query_result.list = 0;
    world->query_result.count = 0;
}

ecs_world *
ecs_world_manager_get_at(
    ecs_world_manager *world_manager,
    size_t world_index)
{
    if(world_index >= world_manager->cap)
    {
        return(0);
    }

    return(&(world_manager->worlds[world_index]));
}

ecs_world *
ecs_world_manager_get(
    ecs_world_manager *world_manager,
    size_t world_id)
{
    size_t *world_index_ptr, world_index;

    world_index_ptr = ecs_map_get(&world_manager->id_to_index, world_id);
    if(!world_index_ptr)
    {
        return(0);
    }

    world_index = *world_index_ptr;

    return(ecs_world_manager_get_at(world_manager, world_index));
}

typedef struct
ecs
{
    ecs_world_manager world_manager;
    size_t current_world_id;
} ecs;

ecs ecs_instance = {0};

size_t
ecs_world_create(void)
{
    size_t world_id, world_index;
    ecs_world world = {0};

    /* TODO: Check index in free_slots first */

    world_id = ++ecs_instance.world_manager.current_id;
    world_index = da_len(ecs_instance.world_manager.worlds);

    da_push(ecs_instance.world_manager.worlds, world);

    ecs_map_set(&ecs_instance.world_manager.id_to_index, world_id, world_index);
    ecs_map_set(&ecs_instance.world_manager.index_to_id, world_index, world_id);

    ecs_instance.world_manager.cap += 1;
    ecs_instance.current_world_id = world_id;

    return(world_id);
}

void
ecs_world_destroy(size_t world_id)
{
    size_t *world_index_ptr, world_index;
    ecs_world *world;

    world_index_ptr = ecs_map_get(&ecs_instance.world_manager.id_to_index, world_id);
    if(!world_index_ptr)
    {
        return;
    }

    world_index = *world_index_ptr;

    world = &(ecs_instance.world_manager.worlds[world_index]);
    world->dead = 1;
}

void
ecs_world_current_set(size_t world_id)
{
    ecs_instance.current_world_id = world_id;
}

size_t
ecs_world_current_get(void)
{
    return(ecs_instance.current_world_id);
}

ecs_query_result*
ecs_query(size_t num_components, ...)
{
    ecs_query_result *result;
    ecs_world *world;
    va_list args;

    world = ecs_world_manager_get(&ecs_instance.world_manager, ecs_instance.current_world_id);
    if(!world || world->dead)
    {
        return(0);
    }

    va_start(args, num_components);
    result = ecs_world_query(world, num_components, args);
    va_end(args);

    return(result);
}

size_t
ecs_entity_create(void)
{
    size_t entity_id;
    ecs_world *world;

    world = ecs_world_manager_get(&ecs_instance.world_manager, ecs_instance.current_world_id);
    if(!world)
    {
        return(0);
    }

    entity_id = ecs_entity_manager_create(&world->entity_manager);

    return(entity_id);
}

void
ecs_entity_destroy(size_t entity_id)
{
    ecs_world *world;
    ecs_entity *entity;

    world = ecs_world_manager_get(&ecs_instance.world_manager, ecs_instance.current_world_id);
    if(!world)
    {
        return;
    }

    entity = ecs_entity_manager_get(&world->entity_manager, entity_id);
    if(!entity)
    {
        return;
    }

    entity->dead = 1;
}

size_t
ecs_component_register(size_t component_size)
{
    size_t component_id;
    ecs_world *world;

    world = ecs_world_manager_get(&ecs_instance.world_manager, ecs_instance.current_world_id);
    if(!world)
    {
        return(0);
    }

    component_id = ecs_component_manager_register(&world->component_manager, component_size);

    return(component_id);
}

void
ecs_component_unregister(size_t component_id)
{
    ecs_world *world;

    world = ecs_world_manager_get(&ecs_instance.world_manager, ecs_instance.current_world_id);
    if(!world)
    {
        return;
    }

    ecs_component_manager_unregister(&world->component_manager, component_id);
}

void
ecs_entity_component_attach(size_t entity_id, size_t component_id)
{
    ecs_world *world;

    world = ecs_world_manager_get(&ecs_instance.world_manager, ecs_instance.current_world_id);
    if(!world)
    {
        return;
    }

    ecs_world_entity_component_attach(world, entity_id, component_id);
}

void
ecs_entity_component_detach(size_t entity_id, size_t component_id)
{
    ecs_world *world;

    world = ecs_world_manager_get(&ecs_instance.world_manager, ecs_instance.current_world_id);
    if(!world)
    {
        return;
    }

    ecs_world_entity_component_detach(world, entity_id, component_id);
}

void*
ecs_entity_component_get(size_t entity_id, size_t component_id)
{
    ecs_world *world;

    world = ecs_world_manager_get(&ecs_instance.world_manager, ecs_instance.current_world_id);
    if(!world)
    {
        return(0);
    }

    return(ecs_world_entity_component_get(world, entity_id, component_id));
}

void
ecs_update(void)
{
    ecs_world *world;
    size_t entity_index, world_index;

    world = ecs_world_manager_get(&ecs_instance.world_manager, ecs_instance.current_world_id);
    if(!world)
    {
        return;
    }

    for(entity_index = 0;
        entity_index < world->entity_manager.cap;
        ++entity_index)
    {
        ecs_entity *entity;

        entity = &(world->entity_manager.entities[entity_index]);
        if(!entity)
        {
            continue;
        }

        if(entity->dead && !entity->destroyed)
        {
            ecs_world_entity_destroy(world, entity->id);
        }
    }

    for(world_index = 0;
        world_index < ecs_instance.world_manager.cap;
        ++world_index)
    {
        world = &(ecs_instance.world_manager.worlds[world_index]);
        if(!world)
        {
            continue;
        }

        if(world->dead && !world->destroyed)
        {
            ecs_world_manager_destroy(&ecs_instance.world_manager, world->id);
        }
    }
}

#endif
#endif
