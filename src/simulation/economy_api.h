#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void economy_add_resource_node(int node_id, float x, float y, float amount, int type);
void economy_add_extractor(int extractor_id, float x, float y, int node_id, float extraction_rate);
void economy_add_storage(int storage_id, float x, float y, float metal_capacity, float energy_capacity, float research_capacity);
void economy_add_production_line(int line_id, int storage_id, float build_speed_metal, float build_speed_energy, int max_jobs);
void economy_enqueue_construction(int line_id, int entity_id, int type, float metal_cost, float energy_cost, float metal_per_tick, float energy_per_tick);
void economy_update_all(float delta_ms);
int economy_get_resource_node_count();
bool economy_get_resource_node_info(int node_id, float* out_x, float* out_y, float* out_amount, int* out_type);
bool economy_get_storage_info(int storage_id, float* out_metal_storage, float* out_energy_storage, float* out_research_storage, 
                              float* out_metal_capacity, float* out_energy_capacity, float* out_research_capacity);
int economy_get_queue_size(int line_id);
int economy_get_completed_build_count();

#ifdef __cplusplus
}
#endif
