#include <Windows.h>
#include "Rsw.h"
#include <browedit/BrowEdit.h>
#include <browedit/Node.h>
#include <browedit/components/RsmRenderer.h>
#include <browedit/util/FileIO.h>
#include <browedit/util/ResourceManager.h>

#include <iostream>
#include <fstream>
#include <json.hpp>
#include <ranges>
#include <glm/gtc/type_ptr.hpp>

RswModel::RswModel(RswModel* other) : aabb(other->aabb), animType(other->animType), animSpeed(other->animSpeed), blockType(other->blockType), fileName(other->fileName)
{
}

void RswModel::load(std::istream* is, int version, unsigned char buildNumber, bool loadModel)
{
	auto rswObject = node->getComponent<RswObject>();
	if (version >= 0x103)
	{
		node->name = util::iso_8859_1_to_utf8(util::FileIO::readString(is, 40));

		is->read(reinterpret_cast<char*>(&animType), sizeof(int));
		is->read(reinterpret_cast<char*>(&animSpeed), sizeof(float));
		is->read(reinterpret_cast<char*>(&blockType), sizeof(int));
	}
	if (version >= 0x0206 && buildNumber > 161)
	{
		unsigned char c = is->get(); // unknown, 0?
	}

	std::string fileNameRaw = util::FileIO::readString(is, 80);
	//std::cout << "Model: " << node->name << "\t" << fileNameRaw << std::endl;
	fileName = util::iso_8859_1_to_utf8(fileNameRaw);
	assert(fileNameRaw == util::utf8_to_iso_8859_1(fileName));
	objectName = util::iso_8859_1_to_utf8(util::FileIO::readString(is, 80)); // TODO: Unknown?
	is->read(reinterpret_cast<char*>(glm::value_ptr(rswObject->position)), sizeof(float) * 3);
	is->read(reinterpret_cast<char*>(glm::value_ptr(rswObject->rotation)), sizeof(float) * 3);
	is->read(reinterpret_cast<char*>(glm::value_ptr(rswObject->scale)), sizeof(float) * 3);
	if (loadModel)
	{
		node->addComponent(util::ResourceManager<Rsm>::load("data\\model\\" + fileNameRaw));
		node->addComponent(new RsmRenderer());
	}
}


void RswModel::loadExtra(nlohmann::json data)
{
	try {
		givesShadow = data["shadow"];
	}
	catch (...) {}
}



void RswModel::save(std::ofstream& file, int version)
{
	auto rswObject = node->getComponent<RswObject>();
	if (version >= 0x103)
	{
		util::FileIO::writeString(file, util::utf8_to_iso_8859_1(node->name), 40);
		file.write(reinterpret_cast<char*>(&animType), sizeof(int));
		file.write(reinterpret_cast<char*>(&animSpeed), sizeof(float));
		file.write(reinterpret_cast<char*>(&blockType), sizeof(int));
	}
	util::FileIO::writeString(file, util::utf8_to_iso_8859_1(fileName), 80);
	util::FileIO::writeString(file, util::utf8_to_iso_8859_1(objectName), 80); //unknown
	file.write(reinterpret_cast<char*>(glm::value_ptr(rswObject->position)), sizeof(float) * 3);
	file.write(reinterpret_cast<char*>(glm::value_ptr(rswObject->rotation)), sizeof(float) * 3);
	file.write(reinterpret_cast<char*>(glm::value_ptr(rswObject->scale)), sizeof(float) * 3);
}


nlohmann::json RswModel::saveExtra()
{
	nlohmann::json ret;
	ret["shadow"] = givesShadow;
	return ret;
}



void RswModel::buildImGuiMulti(BrowEdit* browEdit, const std::vector<Node*>& nodes)
{
	std::vector<RswModel*> rswModels;
	std::ranges::copy(nodes | std::ranges::views::transform([](Node* n) { return n->getComponent<RswModel>(); }) | std::ranges::views::filter([](RswModel* r) { return r != nullptr; }), std::back_inserter(rswModels));
	if (rswModels.size() == 0)
		return;

	ImGui::Text("Model");
	ImGui::PushID("Model");
	if (ImGui::BeginPopupContextItem("CopyPaste"))
	{
		try {
			if (ImGui::MenuItem("Copy"))
			{
				json clipboard;
				to_json(clipboard, *rswModels[0]);
				ImGui::SetClipboardText(clipboard.dump(1).c_str());
			}
			if (ImGui::MenuItem("Paste (no undo)"))
			{
				auto cb = ImGui::GetClipboardText();
				if (cb)
					for (auto rswModel : rswModels)
					{
						from_json(json::parse(std::string(cb)), *rswModel);

						auto removed = rswModel->node->removeComponent<Rsm>();
						for (auto r : removed)
							util::ResourceManager<Rsm>::unload(r);
						rswModel->node->addComponent(util::ResourceManager<Rsm>::load("data\\model\\" + util::utf8_to_iso_8859_1(rswModel->fileName)));
						rswModel->node->getComponent<RsmRenderer>()->begin();
						rswModel->node->getComponent<RswModelCollider>()->begin();
					}
			}
		}
		catch (...) {}
		ImGui::EndPopup();
	}
	ImGui::PopID();

	util::DragIntMulti<RswModel>(browEdit, browEdit->activeMapView->map, rswModels, "Animation Type", [](RswModel* m) { return &m->animType; }, 1, 0, 100);
	util::DragFloatMulti<RswModel>(browEdit, browEdit->activeMapView->map, rswModels, "Animation Speed", [](RswModel* m) { return &m->animSpeed; }, 0.01f, 0.0f, 100.0f);
	util::DragIntMulti<RswModel>(browEdit, browEdit->activeMapView->map, rswModels, "Block Type", [](RswModel* m) { return &m->blockType; }, 1, 0, 100);
	if (util::InputTextMulti<RswModel>(browEdit, browEdit->activeMapView->map, rswModels, "Filename", [](RswModel* m) { return &m->fileName; }))
		for (auto m : rswModels)
			if (util::utf8_to_iso_8859_1(m->fileName).size() > 80)
				m->fileName = util::iso_8859_1_to_utf8(util::utf8_to_iso_8859_1(m->fileName).substr(0, 80));
	if(util::InputTextMulti<RswModel>(browEdit, browEdit->activeMapView->map, rswModels, "ObjectName", [](RswModel* m) { return &m->objectName; }))
		for (auto m : rswModels)
			if (util::utf8_to_iso_8859_1(m->objectName).size() > 80)
				m->objectName = util::iso_8859_1_to_utf8(util::utf8_to_iso_8859_1(m->objectName).substr(0, 80));

	util::CheckboxMulti<RswModel>(browEdit, browEdit->activeMapView->map, rswModels, "Cast Shadow", [](RswModel* m) { return &m->givesShadow; });
}
