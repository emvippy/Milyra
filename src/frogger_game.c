#include "frogger_game.h"

#include "debug.h"
#include "ecs.h"
#include "fs.h"
#include "gpu.h"
#include "heap.h"
#include "render.h"
#include "timer_object.h"
#include "transform.h"
#include "wm.h"
#include "audio.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

typedef struct transform_component_t
{
	transform_t transform;
} transform_component_t;

typedef struct camera_component_t
{
	mat4f_t projection;
	mat4f_t view;
} camera_component_t;

typedef struct model_component_t
{
	gpu_mesh_info_t* mesh_info;
	gpu_shader_info_t* shader_info;
} model_component_t;

typedef struct player_component_t
{
	int index;
} player_component_t;

typedef struct enemy_component_t 
{
	int index;
} enemy_component_t;

typedef struct name_component_t
{
	char name[32];
} name_component_t;

typedef struct frogger_game_t
{
	heap_t* heap;
	fs_t* fs;
	wm_window_t* window;
	render_t* render;
	audio_t* audio;

	timer_object_t* timer;

	ecs_t* ecs;
	int transform_type;
	int camera_type;
	int model_type;
	int player_type;
	int truck1_type;
	int truck2_type;
	int truck3_type;
	int name_type;
	ecs_entity_ref_t player_ent;
	ecs_entity_ref_t truck1_ent;
	ecs_entity_ref_t truck2_ent;
	ecs_entity_ref_t truck3_ent;
	ecs_entity_ref_t camera_ent;

	gpu_mesh_info_t cube_mesh;
	gpu_mesh_info_t enemy_mesh;
	gpu_shader_info_t cube_shader;
	gpu_shader_info_t enemy_shader;
	fs_work_t* vertex_shader_work;
	fs_work_t* fragment_shader_work;
} frogger_game_t;

static void load_resources(frogger_game_t* game);
static void unload_resources(frogger_game_t* game);
static void spawn_player(frogger_game_t* game, int index);
static void spawn_truck(frogger_game_t* game, int index);
static void spawn_camera(frogger_game_t* game);
static void update_players(frogger_game_t* game);
static void update_trucks(frogger_game_t* game);
static void draw_models(frogger_game_t* game);

//HELPER FUNCTIONS FOR MATH
float float_rand(float min, float max)
{
	float scale = rand() / (float)RAND_MAX; /* [0, 1.0] */
	return min + scale * (max - min);      /* [min, max] */
}
bool TestAABBCollision(transform_component_t* a, transform_component_t* b)
{
	// Since we're using 2D collision detection, only use yz coordinates
	float a_max_y = a->transform.translation.y + (a->transform.scale.y);
	float a_min_y = a->transform.translation.y - (a->transform.scale.y);
	float b_max_y = b->transform.translation.y + (b->transform.scale.y);
	float b_min_y = b->transform.translation.y - (b->transform.scale.y);
	float a_max_z = a->transform.translation.z + (a->transform.scale.z);
	float a_min_z = a->transform.translation.z - (a->transform.scale.z);
	float b_max_z = b->transform.translation.z + (b->transform.scale.z);
	float b_min_z = b->transform.translation.z - (b->transform.scale.z);

	float d1x = b_min_y - a_max_y;
	float d1y = b_min_z - a_max_z;
	float d2x = a_min_y - b_max_y;
	float d2y = a_min_z - b_max_z;

	if (d1x > 0.0f || d1y > 0.0f)
		return false;

	if (d2x > 0.0f || d2y > 0.0f)
		return false;

	return true;
}

frogger_game_t* frogger_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render, audio_t* audio, int argc, const char** argv)
{
	frogger_game_t* game = heap_alloc(heap, sizeof(frogger_game_t), 8);
	game->heap = heap;
	game->fs = fs;
	game->window = window;
	game->render = render;
	game->audio = audio;

	game->timer = timer_object_create(heap, NULL);

	game->ecs = ecs_create(heap);
	game->transform_type = ecs_register_component_type(game->ecs, "transform", sizeof(transform_component_t), _Alignof(transform_component_t));
	game->camera_type = ecs_register_component_type(game->ecs, "camera", sizeof(camera_component_t), _Alignof(camera_component_t));
	game->model_type = ecs_register_component_type(game->ecs, "model", sizeof(model_component_t), _Alignof(model_component_t));
	game->player_type = ecs_register_component_type(game->ecs, "player", sizeof(player_component_t), _Alignof(player_component_t));
	game->truck1_type = ecs_register_component_type(game->ecs, "truck1", sizeof(enemy_component_t), _Alignof(enemy_component_t));
	game->truck2_type = ecs_register_component_type(game->ecs, "truck2", sizeof(enemy_component_t), _Alignof(enemy_component_t));
	game->truck3_type = ecs_register_component_type(game->ecs, "truck3", sizeof(enemy_component_t), _Alignof(enemy_component_t));
	game->name_type = ecs_register_component_type(game->ecs, "name", sizeof(name_component_t), _Alignof(name_component_t));


	load_resources(game);
	spawn_player(game, 0);
	spawn_truck(game, 1);
	spawn_camera(game);

	return game;
}

void frogger_game_destroy(frogger_game_t* game)
{
	ecs_destroy(game->ecs);
	timer_object_destroy(game->timer);
	unload_resources(game);
	heap_free(game->heap, game);
}

void frogger_game_update(frogger_game_t* game)
{
	timer_object_update(game->timer);
	ecs_update(game->ecs);
	update_players(game);
	update_trucks(game);
	draw_models(game);
	render_push_done(game->render);
}

static void load_resources(frogger_game_t* game)
{
	game->vertex_shader_work = fs_read(game->fs, "shaders/triangle.vert.spv", game->heap, false, false);
	game->fragment_shader_work = fs_read(game->fs, "shaders/triangle.frag.spv", game->heap, false, false);
	game->cube_shader = (gpu_shader_info_t)
	{
		.vertex_shader_data = fs_work_get_buffer(game->vertex_shader_work),
		.vertex_shader_size = fs_work_get_size(game->vertex_shader_work),
		.fragment_shader_data = fs_work_get_buffer(game->fragment_shader_work),
		.fragment_shader_size = fs_work_get_size(game->fragment_shader_work),
		.uniform_buffer_count = 1,
	};

	game->enemy_shader = (gpu_shader_info_t)
	{
		.vertex_shader_data = fs_work_get_buffer(game->vertex_shader_work),
		.vertex_shader_size = fs_work_get_size(game->vertex_shader_work),
		.fragment_shader_data = fs_work_get_buffer(game->fragment_shader_work),
		.fragment_shader_size = fs_work_get_size(game->fragment_shader_work),
		.uniform_buffer_count = 1,
	};

	static vec3f_t cube_verts[] =
	{
		{ -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f,  0.0f },
		{ -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f,  0.0f },
		{ -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
		{ -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
	};
	static uint16_t cube_indices[] =
	{
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		7, 6, 5,
		5, 4, 7,
		4, 0, 3,
		3, 7, 4,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};
	game->cube_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts,
		.vertex_data_size = sizeof(cube_verts),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};

	static vec3f_t enemy_verts[] =
	{
		{ -1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f,  0.0f },
		{  1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f,  1.0f },
		{  1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f,  0.0f },
		{ -1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f,  1.0f },
		{ -1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f,  0.0f },
		{  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f,  1.0f },
		{  1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f,  1.0f },
		{ -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f,  0.0f },
	};
	static uint16_t enemy_indices[] =
	{
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		7, 6, 5,
		5, 4, 7,
		4, 0, 3,
		3, 7, 4,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};
	game->enemy_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = enemy_verts,
		.vertex_data_size = sizeof(enemy_verts),
		.index_data = enemy_indices,
		.index_data_size = sizeof(enemy_indices),
	};
}

static void unload_resources(frogger_game_t* game)
{
	heap_free(game->heap, fs_work_get_buffer(game->vertex_shader_work));
	heap_free(game->heap, fs_work_get_buffer(game->fragment_shader_work));
	fs_work_destroy(game->fragment_shader_work);
	fs_work_destroy(game->vertex_shader_work);
}

static void spawn_player(frogger_game_t* game, int index)
{
	uint64_t k_player_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->player_type) |
		(1ULL << game->name_type);
	game->player_ent = ecs_entity_add(game->ecs, k_player_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);
	transform_comp->transform.translation.z = 12.0f;

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "player");

	player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);
	player_comp->index = index;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;

}

static void spawn_truck(frogger_game_t* game, int index)
{
	uint64_t k_truck1_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->truck1_type) |
		(1ULL << game->name_type);

	for (int i = 0; i < 6; i++) {
		game->truck1_ent = ecs_entity_add(game->ecs, k_truck1_ent_mask);

		transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->truck1_ent, game->transform_type, true);
		transform_identity(&transform_comp->transform);
		transform_comp->transform.translation.z = 8.0f;
		transform_comp->transform.translation.y = (8.0f * i) - 24.0f;
		transform_comp->transform.scale.y = float_rand(1.4f, 2.0f);

		name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->truck1_ent, game->name_type, true);
		strcpy_s(name_comp->name, sizeof(name_comp->name), "truck1");

		player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->truck1_ent, game->player_type, true);
		player_comp->index = index;

		model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->truck1_ent, game->model_type, true);
		model_comp->mesh_info = &game->enemy_mesh;
		model_comp->shader_info = &game->enemy_shader;
	}

	uint64_t k_truck2_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->truck2_type) |
		(1ULL << game->name_type);

	for (int i = 0; i < 4; i++) {
		game->truck2_ent = ecs_entity_add(game->ecs, k_truck2_ent_mask);

		transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->truck2_ent, game->transform_type, true);
		transform_identity(&transform_comp->transform);
		transform_comp->transform.translation.y = (12.0f * i) - 24.0f;
		transform_comp->transform.scale.y = float_rand(1.8f, 2.8f);

		name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->truck2_ent, game->name_type, true);
		strcpy_s(name_comp->name, sizeof(name_comp->name), "truck2");

		player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->truck2_ent, game->player_type, true);
		player_comp->index = index;

		model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->truck2_ent, game->model_type, true);
		model_comp->mesh_info = &game->enemy_mesh;
		model_comp->shader_info = &game->enemy_shader;
	}

	uint64_t k_truck3_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->truck3_type) |
		(1ULL << game->name_type);

	for (int i = 0; i < 3; i++) {
		game->truck3_ent = ecs_entity_add(game->ecs, k_truck3_ent_mask);

		transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->truck3_ent, game->transform_type, true);
		transform_identity(&transform_comp->transform);
		transform_comp->transform.translation.z = -8.0f;
		transform_comp->transform.translation.y = (16.0f * i) - 24.0f;
		transform_comp->transform.scale.y = float_rand(1.8f, 4.0f);

		name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->truck3_ent, game->name_type, true);
		strcpy_s(name_comp->name, sizeof(name_comp->name), "truck3");

		player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->truck3_ent, game->player_type, true);
		player_comp->index = index;

		model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->truck3_ent, game->model_type, true);
		model_comp->mesh_info = &game->enemy_mesh;
		model_comp->shader_info = &game->enemy_shader;
	}

}

static void spawn_camera(frogger_game_t* game)
{
	uint64_t k_camera_ent_mask =
		(1ULL << game->camera_type) |
		(1ULL << game->name_type);
	game->camera_ent = ecs_entity_add(game->ecs, k_camera_ent_mask);

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "camera");

	camera_component_t* camera_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->camera_type, true);
	mat4f_make_orthographic(&camera_comp->projection, 16.0f, 9.0f, 0.1f, 15.0f);

	vec3f_t eye_pos = vec3f_scale(vec3f_forward(), -5.0f);
	vec3f_t forward = vec3f_forward();
	vec3f_t up = vec3f_up();
	mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
}

static void update_players(frogger_game_t* game)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.005f;

	uint32_t key_mask = wm_get_key_mask(game->window);

	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->player_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		player_component_t* player_comp = ecs_query_get_component(game->ecs, &query, game->player_type);

		// Respawn box
		if (transform_comp->transform.translation.z < -12.0f)
		{
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &query), false);
			spawn_player(game, 0);
		}

		transform_t move;
		transform_identity(&move);
		if (key_mask & k_key_up)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), -dt));
		}
		if (key_mask & k_key_down)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), dt));
		}
		if (key_mask & k_key_left)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -dt));
		}
		if (key_mask & k_key_right)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), dt));
		}
		transform_multiply(&transform_comp->transform, &move);
	}
}

static void update_trucks(frogger_game_t* game)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.002f;

	//Get player for collision detection
	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->player_type);
	ecs_query_t player_query = ecs_query_create(game->ecs, k_query_mask);
	transform_component_t* player_transform_comp = ecs_query_get_component(game->ecs, &player_query, game->transform_type);

	uint64_t k_query_mask1 = (1ULL << game->transform_type) | (1ULL << game->truck1_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask1);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);

		transform_comp->transform.translation.y += dt;

		if (transform_comp->transform.translation.y > 24.0f) {
			transform_comp->transform.translation.y *= -1.0f;
		}
		if (TestAABBCollision(transform_comp, player_transform_comp)) {
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &player_query), false);
			play_sound_buffer(game->audio);
			spawn_player(game, 0);
		}
	}

	uint64_t k_query_mask2 = (1ULL << game->transform_type) | (1ULL << game->truck2_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask2);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);

		transform_comp->transform.translation.y += (dt * 2.0f);

		if (transform_comp->transform.translation.y > 24.0f) {
			transform_comp->transform.translation.y *= -1.0f;
		}

		if (TestAABBCollision(transform_comp, player_transform_comp)) {
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &player_query), false);
			play_sound_buffer(game->audio);
			spawn_player(game, 0);
		}
	}

	uint64_t k_query_mask3 = (1ULL << game->transform_type) | (1ULL << game->truck3_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask3);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);

		transform_comp->transform.translation.y += (dt * 4.0f);

		if (transform_comp->transform.translation.y > 24.0f) {
			transform_comp->transform.translation.y *= -1.0f;
		}

		if (TestAABBCollision(transform_comp, player_transform_comp)) {
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &player_query), false);
			play_sound_buffer(game->audio);
			spawn_player(game, 0);
		}
	}
}

static void draw_models(frogger_game_t* game)
{
	uint64_t k_camera_query_mask = (1ULL << game->camera_type);
	for (ecs_query_t camera_query = ecs_query_create(game->ecs, k_camera_query_mask);
		ecs_query_is_valid(game->ecs, &camera_query);
		ecs_query_next(game->ecs, &camera_query))
	{
		camera_component_t* camera_comp = ecs_query_get_component(game->ecs, &camera_query, game->camera_type);

		uint64_t k_model_query_mask = (1ULL << game->transform_type) | (1ULL << game->model_type);
		for (ecs_query_t query = ecs_query_create(game->ecs, k_model_query_mask);
			ecs_query_is_valid(game->ecs, &query);
			ecs_query_next(game->ecs, &query))
		{
			transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
			model_component_t* model_comp = ecs_query_get_component(game->ecs, &query, game->model_type);
			ecs_entity_ref_t entity_ref = ecs_query_get_entity(game->ecs, &query);

			struct
			{
				mat4f_t projection;
				mat4f_t model;
				mat4f_t view;
			} uniform_data;
			uniform_data.projection = camera_comp->projection;
			uniform_data.view = camera_comp->view;
			transform_to_matrix(&transform_comp->transform, &uniform_data.model);
			gpu_uniform_buffer_info_t uniform_info = { .data = &uniform_data, sizeof(uniform_data) };

			render_push_model(game->render, &entity_ref, model_comp->mesh_info, model_comp->shader_info, &uniform_info);
		}
	}
}
