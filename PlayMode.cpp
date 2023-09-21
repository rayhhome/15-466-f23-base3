#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <cassert>

// Load data
GLuint stage_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > stage_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("data/stage.pnct"));
	stage_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > stage_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("data/stage.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = stage_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = stage_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > alarm_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("data/alarm.wav"));
});

Load< Sound::Sample > background_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("data/desertian.wav"));
});

Load< Sound::Sample > hit_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("data/hit.wav"));
});

// PlayMode constructor
PlayMode::PlayMode() : scene(*stage_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Explosion") explosion = &transform;
	}
	if (explosion == nullptr) throw std::runtime_error("Explosion not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//also get pointer to light
	if (scene.lights.size() != 1) throw std::runtime_error("Expecting scene to have exactly one light, but it has " + std::to_string(scene.lights.size()) + ".");
	light = &scene.lights.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	background_loop = Sound::loop(*background_sample, 0.7f, 0.0f);

	//assure prepare state is true
	prepare = true;

	//assure start timer state is true
	start_timer = true;

	//initialize timer
	timer = 0.0f;

	//initialize hit flag
	hit = false;
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_k) {
			ccw.downs += 1;
			ccw.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_l) {
			cw.downs += 1;
			cw.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_k) {
			ccw.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_l) {
			cw.pressed = false;
			return true;
		}
	} 
	return false;
}

void PlayMode::update(float elapsed) {
	//update timer
	timer += elapsed;

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 20.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
		if (camera->transform->position.x > max_x) {
			camera->transform->position.x = max_x;
		} else if (camera->transform->position.x < -max_x) {
			camera->transform->position.x = -max_x;
		}
		if (camera->transform->position.y > max_y) {
			camera->transform->position.y = max_y;
		} else if (camera->transform->position.y < -max_y) {
			camera->transform->position.y = -max_y;
		}
		
		constexpr float PlayerRotationSpeed = 40.0f;
		if (ccw.pressed && !cw.pressed) {
			camera->transform->rotation = camera->transform->rotation * glm::angleAxis(
				glm::radians(PlayerRotationSpeed * elapsed),
				glm::vec3(0.0f, 1.0f, 0.0f)
			);
		} else if (!ccw.pressed && cw.pressed) {
			camera->transform->rotation = camera->transform->rotation * glm::angleAxis(
				glm::radians(-PlayerRotationSpeed * elapsed),
				glm::vec3(0.0f, 1.0f, 0.0f)
			);
		}

	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}


	{ //update explosion position
		if (start_timer) {
			if (prepare) {
				//set oncoming explosion position and audio accordingly
				// From https://gamedev.stackexchange.com/questions/179426/c-generate-random-float-values-between-a-range#:~:text=rand()%20%25%20200%20yields%20a,200%20becomes%20%2D200%20to%200.&text=And%20now%20this%20code%20correctly%20generates%20floats%20between%20%2D200%20and%20200.
				explosion_pos.x = -30 + static_cast <float>(rand())/(static_cast <float> (RAND_MAX/(30-(-30))));
				explosion_pos.y = -30 + static_cast <float>(rand())/(static_cast <float> (RAND_MAX/(30-(-30))));

				alarm_audio = Sound::play_3D(*alarm_sample, 1.0f, glm::vec3(explosion_pos.x, explosion_pos.y, 2.0f), 20.0f);
				
				//next state actually explodes when timer hits
				prepare = false;
			} else {
				explosion->position.x = explosion_pos.x;
				explosion->position.y = explosion_pos.y;
				float cur_scale = (1.0f + 0.1f * (points - min_points));
				explosion->scale = glm::vec3(cur_scale, cur_scale, cur_scale);

				//next state prepares next explosion
				prepare = true;
			}
			start_timer = false;
		} else {
			if ((prepare && (timer > 1.0f)) || 
					((!prepare && (timer > 5.0f)))) {
				//need to reset timer
				timer = 0.0f;
				start_timer = true;

				//move explosion to default position
				explosion->position.x = 50.0f;
				explosion->position.y = 0.0f;

				//reset hit flag if we are setting oncoming explosion
				if (prepare) {
					if (hit) {
						if (points > min_points) {
							points -= 1;
						}
					} else {
						if (points < max_points) {
							points += 1;
						}
					}
					hit = false;
				}
			}
		}
	}

	{ //detect hit
		float difficulty = (1.0f + 0.1f * (points - min_points));
		glm::vec3 camera_pos = camera->transform->position;
		glm::vec3 explosion_pos = explosion->position;
		if ((camera_pos.x - explosion_pos.x) * (camera_pos.x - explosion_pos.x) + 
				(camera_pos.y - explosion_pos.y) * (camera_pos.y - explosion_pos.y) < 
				(hit_radius * hit_radius) * difficulty * difficulty) {
				if (!(hit)) {
					//play hit sample
					hit_audio = Sound::play(*hit_sample, 1.0f, 0.0f);
					hit = true;
				}
		}

	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	cw.downs = 0;
	ccw.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,30.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));	glUseProgram(0);

	glClearColor(0.4f, 0.7f, 0.7f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("WASD to move; KL to turn.",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("WASD to move; KL to turn.",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		float line = 100.0f / drawable_size.y;
		lines.draw_text("Your current points: " + std::to_string(points),
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H + line, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("Your current points: " + std::to_string(points),
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs + line, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		lines.draw_text("Ekrixiphobia the Game!!!!!!!",
			glm::vec3(-aspect + 0.1f * H, 1.0 - 0.1f * H - line, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("Ekrixiphobia the Game!!!!!!!",
			glm::vec3(-aspect + 0.1f * H + ofs, 1.0 - 0.1f * H + ofs - line, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}