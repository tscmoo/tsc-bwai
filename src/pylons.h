

namespace pylons {
;

a_vector<xy> existing_completed_pylons;

bool is_in_pylon_range(xy rel) {
	static const bool bPsiFieldMask[10][16] = {
		{ 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 },
		{ 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 },
		{ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 },
		{ 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 },
		{ 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 }
	};
	if (std::abs(rel.x)>=256) return false;
	if (std::abs(rel.y)>=160) return false;
	return bPsiFieldMask[(unsigned)(rel.y + 160)/32][(unsigned)(rel.x + 256)/32];
}

template<typename cont_T>
bool is_in_pylon_range(xy pos,const cont_T&cont) {
	for (auto&v : cont) {
		if (is_in_pylon_range(pos-v)) return true;
	}
	return false;
}


void update_pylons_task() {
	
	while (true) {

		existing_completed_pylons.clear();
		for (unit*u : my_buildings) {
			if (u->type->game_unit_type!=BWAPI::UnitTypes::Protoss_Pylon) continue;
			existing_completed_pylons.push_back(u->pos);
		}

		multitasking::sleep(1);
	}

}

void render() {

	int w = BWAPI::UnitTypes::Protoss_Gateway.tileWidth();
	int h = BWAPI::UnitTypes::Protoss_Gateway.tileHeight();

// 	for (int y=0;y<grid::build_grid_height;++y) {
// 		for (int x=0;x<grid::build_grid_width;++x) {
// 			xy pos(x*32+w*16,y*32+h*16);
// 			if (is_in_pylon_range(pos,existing_pylons)) {
// 				game->drawBoxMap(x*32,y*32,x*32+w*32,y*32+h*32,BWAPI::Colors::Orange);
// 			}
// 
// 		}
// 	}


}

void init() {

	multitasking::spawn(update_pylons_task,"update pylons");

	render::add(render);

}


}

