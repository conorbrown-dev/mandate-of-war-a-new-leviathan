#pragma once

#ifdef __cplusplus
extern "C" {
#endif

bool combat_get_unit_health(int entity_id, float* out_current, float* out_max);
int combat_get_unit_is_dead(int entity_id);
bool combat_apply_damage(int entity_id, float damage);
int combat_get_unit_faction_id(int entity_id);

#ifdef __cplusplus
}
#endif
