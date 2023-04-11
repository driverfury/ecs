# ECS

A simple, lightweight Entity Component System.

## Usage:

Include the header file:

```c
#define ECS_IMPLEMENTATION
#include "ecs.h"
```

Create a world and register the components you want:

```c

typedef struct
{
    float x;
    float y;
    float z;
} position;

typdef struct
{
    float x;
    float y;
    float z;
} velocity;

typedef struct
{
    void *image;
    int width;
    int height;
} sprite;

size_t position_component;
size_t velocity_component;
size_t sprite_component;

void init_world_and_components()
{
    ecs_world_create();

    position_component = ecs_component_register(sizeof(position));
    velocity_component = ecs_component_register(sizeof(velocity));
    sprite_component = ecs_component_register(sizeof(sprite));
}

```

Then you can create the entities:

```c
void create_entities()
{
    size_t player = ecs_entity_create();

    ecs_entity_component_attach(player, position_component);
    ecs_entity_component_attach(player, velocity_component);
    ecs_entity_component_attach(player, sprite_component);

    position *p = (position *)ecs_entity_component_get(player, position_component);
    velocity *v = (velocity *)ecs_entity_component_get(player, velocity_component);
    sprite *s = (sprite *)ecs_entity_component_get(player, sprite_component);

    /* Initialize player position */
    p->x = ...;
    p->y = ...;
    p->z = ...;

    /* Initialize player velocity */
    v->x = ...;
    v->y = ...;
    v->z = ...;

    /* Initialize player sprite */
    p->image = ...;
    p->width = ...;
    p->height = ...;
}
```

You also need systems: functions that operate on components.

```c
void system_move(float dt)
{
    ecs_query_result *query;
    size_t i;
    
    query = ecs_query(2, position_component, velocity_component);
    if(!query)
    {
        return;
    }

    for(i = 0;
        i < query->count;
        ++i)
    {
        position *p = (position *)query->list[i][0];
        velocity *v = (velocity *)query->list[i][1];
        p->x += v->x * dt;
        p->y += v->y * dt;
        p->z += v->z * dt;
    }
}

void system_draw_sprites()
{
    ecs_query_result *query;
    size_t i;
    
    query = ecs_query(2, position_component, sprite_component);
    if(!query)
    {
        return;
    }

    for(i = 0;
        i < query->count;
        ++i)
    {
        position *p = (position *)query->list[i][0];
        sprite *s = (sprite *)query->list[i][1];

        draw_image(s->image, p->x, p->y);
    }
}
```

And then the game main loop:

```c
int main()
{
    init_world_and_components();
    create_entities();

    /* Main loop */
    while(game_is_running)
    {
        float dt = ...; /* Get elapsed time */

        system_move(dt);
        system_draw_sprites();

        ecs_update();
    }
}
```
