#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, cw, ccw;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//max position of camera
	const float max_x = 29.5f;
	const float max_y = 29.5f;

	//prepare state
	bool prepare = true;

	//start timer?
	bool start_timer = true;
	float timer = 0.0f;

	//hit flag
	bool hit = false;

	//hit radius
	const float hit_radius = 10.0f;

	//point system
	int points = 0;
	const int min_points = 0;
	const int max_points = 10;

	//pointer to transforms
	Scene::Transform *explosion = nullptr;

	//oncoming explosion position
	glm::vec2 explosion_pos;

	//background music
	std::shared_ptr< Sound::PlayingSample > background_loop;

	//alarm
	std::shared_ptr< Sound::PlayingSample > alarm_audio;

	//hit noise
	std::shared_ptr< Sound::PlayingSample > hit_audio;
	
	//camera:
	Scene::Camera *camera = nullptr;

	//light:
	Scene::Light *light = nullptr;

};
