#include "zone.h"

Zone::Zone(const std::string &name)
:
m_name(name),
m_camera(RES_X, RES_Y, 45.0f, 0.1f, 15000.0f),
#ifndef EQEMU_GL_DEP
m_shader("shaders/basic.vert", "shaders/basic.frag")
#else
m_shader("shaders/basic130.vert", "shaders/basic130.frag")
#endif
{
}

void Zone::Load()
{
	m_uniform = m_shader.GetUniformLocation("MVP");
	m_tint = m_shader.GetUniformLocation("Tint");
	Model *collide = nullptr;
	Model *invis = nullptr;
	Model *volume = nullptr;

	LoadMap(m_name, &collide, &invis);
	LoadWaterMap(m_name, &volume);

	if(collide == nullptr)
		eqLogMessage(LogWarn, "Couldn't load zone geometry from %s.map.", m_name.c_str());

	if(volume == nullptr)
		eqLogMessage(LogWarn, "Couldn't load zone volumes from %s.wtr, either does not exist or is a version 1 wtr file which can't be displayed.", m_name.c_str());

	m_collide.reset(collide);
	m_invis.reset(invis);
	m_volume.reset(volume);

	z_map = std::unique_ptr<ZoneMap>(ZoneMap::LoadMapFile(m_name));
	w_map = std::unique_ptr<WaterMap>(WaterMap::LoadWaterMapfile(m_name));
}

void Zone::Render(bool r_c, bool r_nc, bool r_vol, bool r_nav) {

	glm::vec3 loc = m_camera.GetLoc();
	{
		glm::vec3 min;
		glm::vec3 max;
		if(m_collide) {
			min = m_collide->GetAABBMin();
		}

		if(z_map) {
			max = m_collide->GetAABBMax();
		}

		ImGui::SetNextWindowSize(ImVec2(640, 120), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Debug");
		ImGui::TextColored(ImVec4(0.3,1.0,0.3,1.0), "W S A D to Move, Hold RMB to rotate. Hold Shift to speed boost. ESC to exit.");
		ImGui::Text("Application average %.2f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Text("Zone: %s, min: (%.2f, %.2f, %.2f), max: (%.2f, %.2f, %.2f)", m_name.c_str(), min.x, min.y, min.z, max.x, max.y, max.z);
		ImGui::Text("%.2f, %.2f, %.2f", loc.x, loc.z, loc.y);
		if(z_map && w_map) {
			ZoneMap::Vertex locv(loc.x, loc.z, loc.y);
			ImGui::Text("Best Z: %.2f, In Liquid: %s", z_map->FindBestZ(locv, nullptr),
						w_map->InLiquid(loc.x, loc.z, loc.y) ? "true" : "false");
		}
		else if(z_map) {
			ZoneMap::Vertex locv(loc.x, loc.z, loc.y);
			ImGui::Text("Best Z: %.2f", z_map->FindBestZ(locv, nullptr));
		}
		else if(w_map) {
			ImGui::Text("In Liquid: %s", w_map->InLiquid(loc.x, loc.z, loc.y) ? "true" : "false");
		}
		ImGui::End();
	}

	{
		ImGui::Begin("Navigation");
		if(ImGui::Button("Add node")) {
			if(!m_nav && m_collide && z_map) {
				m_nav = std::unique_ptr<Navigation>(new Navigation(z_map.get(), w_map.get(), m_collide->GetAABBMin(), m_collide->GetAABBMax()));
			}

			ZoneMap::Vertex locv(loc.x, loc.z, loc.y);
			float best_z = z_map->FindBestZ(locv, nullptr);
			if(best_z != BEST_Z_INVALID) {
				m_nav->AddNode(loc.x, loc.z, best_z);
				m_nav->BuildNodeModel();
			}
		}
		ImGui::End();
	}

	glDisable(GL_BLEND);
	m_shader.Use();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glm::mat4 model = glm::mat4(1.0);
	glm::mat4 mvp = m_camera.GetProjMat() * m_camera.GetViewMat() * model;
	m_uniform.SetValueMatrix4(1, false, &mvp[0][0]);

	glm::vec4 tnt(0.8f, 0.8f, 0.8f, 1.0f);
	m_tint.SetValuePtr4(1, &tnt[0]);

	if(m_collide && r_c)
		m_collide->Draw();

	tnt[0] = 0.5f;
	tnt[1] = 0.7f;
	tnt[2] = 1.0f;
	m_tint.SetValuePtr4(1, &tnt[0]);

	if(m_invis && r_nc)
		m_invis->Draw();

	if(r_nav && m_nav) {
		Model *m = m_nav->GetNodesModel();
		if(m) {
			tnt[0] = 0.5f;
			tnt[1] = 1.0f;
			tnt[2] = 0.7f;
			m_tint.SetValuePtr4(1, &tnt[0]);

			m->Draw();
		}
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	tnt[0] = 0.0f;
	tnt[1] = 0.0f;
	tnt[2] = 0.8f;
	tnt[3] = 0.2f;
	m_tint.SetValuePtr4(1, &tnt[0]);

	if(m_volume && r_vol)
		m_volume->Draw();

	glDisable(GL_BLEND);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	tnt[0] = 0.0f;
	tnt[1] = 0.0f;
	tnt[2] = 0.0f;
	tnt[3] = 0.0f;
	m_tint.SetValuePtr4(1, &tnt[0]);

	if(m_collide && r_c)
		m_collide->Draw();

	if(m_invis && r_nc)
		m_invis->Draw();

	if(m_volume && r_vol)
		m_volume->Draw();

	if(r_nav && m_nav) {
		Model *m = m_nav->GetNodesModel();
		if(m) {
			m->Draw();
		}
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
