/* libdogma
 * Copyright (C) 2013 Romain "Artefact2" Dalmaso <artefact2@gmail.com>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "dogma.h"
#include "dogma-extra.h"
#include "dogma-names.h"
#include "dogma_internal.h"
#include "attribute.h"
#include "tables.h"
#include <assert.h>
#include <math.h>

#define DOGMA_Active_OR_HIGHER (DOGMA_STATE_Active &	\
                                (~(DOGMA_STATE_Offline | DOGMA_STATE_Online)))
#define DOGMA_Overload_OR_HIGHER (DOGMA_STATE_Overloaded &			\
                                  (~(DOGMA_STATE_Offline | DOGMA_STATE_Online | DOGMA_STATE_Active)))

int dogma_get_affectors(dogma_context_t* ctx, dogma_location_t loc, dogma_simple_affector_t** list, size_t* len) {
	size_t num_affectors = 0, i = 0;
	dogma_simple_affector_t* affectors;
	dogma_env_t* current_env;
	dogma_env_t* loc_env;
	dogma_fleet_context_t* fleet;
	dogma_array_t *modifiers1, *modifiers2;
	dogma_modifier_t** modifier;
	dogma_key_t attributeid;
	dogma_key_t assoctype;
	dogma_key_t index;
	bool applicable;

	DOGMA_ASSUME_OK(dogma_get_location_env(ctx, loc, &loc_env));

	/* Code below is somewhat similar to the traverse code in attribute.c */

	/* Pass 1: count the number of affecting modifiers */
	current_env = loc_env;

	while(current_env != NULL) {
		attributeid = 0;
		JLF(modifiers1, current_env->modifiers, attributeid);

		while(modifiers1 != NULL) {
			assoctype = 0;
			JLF(modifiers2, *modifiers1, assoctype);

			while(modifiers2 != NULL) {
				index = 0;
				JLF(modifier, *modifiers2, index);

				while(modifier != NULL) {
					DOGMA_ASSUME_OK(dogma_modifier_is_applicable(ctx, loc_env, *modifier, &applicable));
					if(applicable) {
						++num_affectors;
					}

					JLN(modifier, *modifiers2, index);
				}

				JLN(modifiers2, *modifiers1, assoctype);
			}

			JLN(modifiers1, current_env->modifiers, attributeid);
		}

		current_env = current_env->parent;
	}

	fleet = ctx->fleet;
	while(fleet != NULL) {
		if(fleet->booster == NULL || fleet->commander == NULL) {
			break;
		}

		attributeid = 0;
		JLF(modifiers1, fleet->booster->gang->modifiers, attributeid);

		while(modifiers1 != NULL) {
			assoctype = 0;
			JLF(modifiers2, *modifiers1, assoctype);

			while(modifiers2 != NULL) {
				index = 0;
				JLF(modifier, *modifiers2, index);

				while(modifier != NULL) {
					DOGMA_ASSUME_OK(dogma_modifier_is_applicable(ctx, loc_env, *modifier, &applicable));
					if(applicable) {
						++num_affectors;
					}

					JLN(modifier, *modifiers2, index);
				}

				JLN(modifiers2, *modifiers1, assoctype);
			}

			JLN(modifiers1, fleet->booster->gang->modifiers, attributeid);
		}

		fleet = fleet->parent;
	}

	/* Step 2: allocate the list */
	affectors = malloc(num_affectors * sizeof(dogma_simple_affector_t));
	dogma_modifier_t* affecting_modifiers[num_affectors];

	/* Step 3: fill the affecting_modifiers array */
	current_env = loc_env;

	while(current_env != NULL) {
		attributeid = 0;
		JLF(modifiers1, current_env->modifiers, attributeid);

		while(modifiers1 != NULL) {
			assoctype = 0;
			JLF(modifiers2, *modifiers1, assoctype);

			while(modifiers2 != NULL) {
				index = 0;
				JLF(modifier, *modifiers2, index);

				while(modifier != NULL) {
					DOGMA_ASSUME_OK(dogma_modifier_is_applicable(ctx, loc_env, *modifier, &applicable));
					if(applicable) {
						affecting_modifiers[i++] = *modifier;
					}

					JLN(modifier, *modifiers2, index);
				}

				JLN(modifiers2, *modifiers1, assoctype);
			}

			JLN(modifiers1, current_env->modifiers, attributeid);
		}

		current_env = current_env->parent;
	}

	fleet = ctx->fleet;
	while(fleet != NULL) {
		if(fleet->booster == NULL || fleet->commander == NULL) {
			break;
		}

		attributeid = 0;
		JLF(modifiers1, fleet->booster->gang->modifiers, attributeid);

		while(modifiers1 != NULL) {
			assoctype = 0;
			JLF(modifiers2, *modifiers1, assoctype);

			while(modifiers2 != NULL) {
				index = 0;
				JLF(modifier, *modifiers2, index);

				while(modifier != NULL) {
					DOGMA_ASSUME_OK(dogma_modifier_is_applicable(ctx, loc_env, *modifier, &applicable));
					if(applicable) {
						affecting_modifiers[i++] = *modifier;
					}

					JLN(modifier, *modifiers2, index);
				}

				JLN(modifiers2, *modifiers1, assoctype);
			}

			JLN(modifiers1, fleet->booster->gang->modifiers, attributeid);
		}

		fleet = fleet->parent;
	}

	assert(i == num_affectors);

	/* Step 4: fill the list */
	for(i = 0; i < num_affectors; ++i) {
		dogma_simple_affector_t* aff = &affectors[i];
		dogma_modifier_t* modifier = affecting_modifiers[i];

		aff->id = modifier->sourceenv->id;
		aff->destid = modifier->targetattribute;
		aff->order = modifier->assoctype;
		aff->flags = 0;

		if(modifier->penalized) aff->flags |= DOGMA_AFFECTOR_PENALIZED;
		if(modifier->singleton) aff->flags |= DOGMA_AFFECTOR_SINGLETON;

		switch(modifier->assoctype) {

		case DOGMA_ASSOC_PreAssignment:
		case DOGMA_ASSOC_PostAssignment:
			aff->operator = '=';
			aff->value = 0.0;
			break;

		case DOGMA_ASSOC_PostPercent:
		case DOGMA_ASSOC_PostMul:
		case DOGMA_ASSOC_PostDiv:
		case DOGMA_ASSOC_PreMul:
		case DOGMA_ASSOC_PreDiv:
			aff->operator = '*';
			aff->value = 1.0;
			break;

		case DOGMA_ASSOC_ModAdd:
			aff->operator = '+';
			aff->value = 0.0;
			break;

		case DOGMA_ASSOC_ModSub:
			aff->operator = '-';
			aff->value = 0.0;
			break;

		}

		DOGMA_ASSUME_OK(dogma_apply_modifier(ctx, modifier, &(aff->value)));
	}

	*list = affectors;
	*len = num_affectors;
	return DOGMA_OK;
}

int dogma_free_affector_list(dogma_simple_affector_t* list) {
	free(list);
	return DOGMA_OK;
}



int dogma_type_has_effect(dogma_typeid_t id, dogma_state_t s, dogma_effectid_t effect, bool* hasit) {
	const dogma_type_effect_t* te;
	int ret;

	ret = dogma_get_type_effect(id, effect, &te);
	if(ret == DOGMA_OK) {
		const dogma_effect_t* e;

		DOGMA_ASSUME_OK(dogma_get_effect(effect, &e));
		*hasit = ((s >> e->category) & 1);
	} else if(ret == DOGMA_NOT_FOUND) {
		*hasit = false;
	} else {
		return ret;
	}

	return DOGMA_OK;
}

int dogma_type_has_active_effects(dogma_typeid_t id, bool* activable) {
	dogma_array_t effects;
	const dogma_type_effect_t** te;
	const dogma_effect_t* e;
	dogma_key_t index = 0;

	DOGMA_ASSUME_OK(dogma_get_type_effects(id, &effects));

	JLF(te, effects, index);
	while(te != NULL) {
		DOGMA_ASSUME_OK(dogma_get_effect((*te)->effectid, &e));
		if((DOGMA_Active_OR_HIGHER >> e->category) & 1) {
			*activable = true;
			return DOGMA_OK;
		}

		JLN(te, effects, index);
	}

	*activable = false;
	return DOGMA_OK;
}

int dogma_type_has_overload_effects(dogma_typeid_t id, bool* overloadable) {
	dogma_array_t effects;
	const dogma_type_effect_t** te;
	const dogma_effect_t* e;
	dogma_key_t index = 0;

	DOGMA_ASSUME_OK(dogma_get_type_effects(id, &effects));

	JLF(te, effects, index);
	while(te != NULL) {
		DOGMA_ASSUME_OK(dogma_get_effect((*te)->effectid, &e));
		if((DOGMA_Overload_OR_HIGHER >> e->category) & 1) {
			*overloadable = true;
			return DOGMA_OK;
		}

		JLN(te, effects, index);
	}

	*overloadable = false;
	return DOGMA_OK;
}

bool dogma_expression_has_currenttarget(dogma_expressionid_t id) {
	const dogma_expression_t* exp;

	if(dogma_get_expression(id, &exp) == DOGMA_OK) {
		if(exp->operand == DOGMA_DEFENVIDX && exp->envidx == DOGMA_ENVIDX_Target) {
			/* Found our target */
			return true;
		}

		return dogma_expression_has_currenttarget(exp->arg1) || dogma_expression_has_currenttarget(exp->arg2);
	}

	return false;
}

int dogma_type_has_projectable_effects(dogma_typeid_t id, bool* out) {
	dogma_array_t effects;
	const dogma_type_effect_t** te;
	const dogma_effect_t* e;
	dogma_key_t index = 0;

	DOGMA_ASSUME_OK(dogma_get_type_effects(id, &effects));

	JLF(te, effects, index);
	while(te != NULL) {
		DOGMA_ASSUME_OK(dogma_get_effect((*te)->effectid, &e));

		/* Sadly some projectable effects have no client expression
		 * and have to be hardcoded here. :-( */
		/* XXX this is probably incomplete */
		if(e->id == EFFECT_EnergyDestabilizationNew
		   || e->id == EFFECT_Leech
		   || e->id == EFFECT_TargetArmorRepair
		   || e->id == EFFECT_ShieldTransfer
		   || e->id == EFFECT_RemoteHullRepair
		   || e->id == EFFECT_EnergyTransfer
		   || dogma_expression_has_currenttarget(e->preexpressionid)) {
			*out = true;
			return DOGMA_OK;
		}

		JLN(te, effects, index);
	}

	*out = false;
	return DOGMA_OK;
}

int dogma_type_base_attribute(dogma_typeid_t id, dogma_attributeid_t att, double* out) {
	return dogma_get_type_attribute(id, att, out);
}



int dogma_get_number_of_module_cycles_before_reload(dogma_context_t* ctx, dogma_key_t loc, int* out) {
	double charges_per_cycle, crystal_uses = 1, charge_volume, module_capacity;
	dogma_env_t* env;
	bool override_chargerate, override_crystalsgetdamaged;
	int ret;

	DOGMA_ASSUME_OK(dogma_get_location_env(
		ctx, (dogma_location_t){ .type = DOGMA_LOC_Module, .module_index = loc }, &env
	));
	DOGMA_ASSUME_OK(dogma_type_has_overridden_attribute(env->id, ATT_ChargeRate, &override_chargerate));

	ret = dogma_get_location_env(
		ctx, (dogma_location_t){ .type = DOGMA_LOC_Charge, .module_index = loc }, &env
	);
	if(ret == DOGMA_NOT_FOUND) {
		/* No charge */
		override_crystalsgetdamaged = false;
	} else if(ret != DOGMA_OK) {
		return ret;
	} else {
		DOGMA_ASSUME_OK(dogma_type_has_overridden_attribute(
			env->id, ATT_CrystalsGetDamaged, &override_crystalsgetdamaged
		));
	}

	if(!override_chargerate && !override_crystalsgetdamaged) {
		/* XXX: modules with scripts override neither but do not use
		 * the charge (despite the chargeRate default being 1). */
		*out = -1;
		return DOGMA_OK;
	}

	DOGMA_ASSUME_OK(dogma_get_module_attribute(ctx, loc, ATT_ChargeRate, &charges_per_cycle));

	if(charges_per_cycle == 0.0) {
		/* Module does not use/consume charges */
		*out = -1;
		return DOGMA_OK;
	}

	ret = dogma_get_charge_attribute(ctx, loc, ATT_Volume, &charge_volume);
	if(ret == DOGMA_NOT_FOUND) {
		charge_volume = 0.0;
	} else if(ret != DOGMA_OK) {
		return ret;
	}

	if(charge_volume == 0.0) {
		*out = -1;
		return DOGMA_OK;
	}

	DOGMA_ASSUME_OK(dogma_get_module_attribute(ctx, loc, ATT_Capacity, &module_capacity));

	if(override_crystalsgetdamaged) {
		/* XXX: assume charge is a crystal */

		double takes_damage;
		DOGMA_ASSUME_OK(dogma_get_charge_attribute(ctx, loc, ATT_CrystalsGetDamaged, &takes_damage));

		if(takes_damage == 0.0) {
			/* Crystal takes no damage */
			*out = -1;
			return DOGMA_OK;
		} else {
			double hp, vol_chance, vol_damage;
			DOGMA_ASSUME_OK(dogma_get_charge_attribute(ctx, loc, ATT_Hp, &hp));
			DOGMA_ASSUME_OK(dogma_get_charge_attribute(
				ctx, loc, ATT_CrystalVolatilityChance, &vol_chance
			));
			DOGMA_ASSUME_OK(dogma_get_charge_attribute(
				ctx, loc, ATT_CrystalVolatilityDamage, &vol_damage
			));

			if(vol_chance == 0.0 || vol_damage == 0.0) {
				*out = -1;
				return DOGMA_OK;
			}

			/* Compute average number of uses */
			crystal_uses = hp / (vol_chance * vol_damage);
		}
	}

	*out = (int)floor(
		crystal_uses * module_capacity / (charge_volume * charges_per_cycle)
	);

	return DOGMA_OK;
}



int dogma_get_nth_type_effect_with_attributes(dogma_typeid_t t, unsigned int n, dogma_effectid_t* out) {
	dogma_array_t effects;
	const dogma_type_effect_t** te;
	const dogma_effect_t* e;
	dogma_key_t index = 0;
	unsigned int i = 0;

	DOGMA_ASSUME_OK(dogma_get_type_effects(t, &effects));
	JLF(te, effects, index);
	while(te != NULL) {
		DOGMA_ASSUME_OK(dogma_get_effect((*te)->effectid, &e));

		if(e->durationattributeid != 0 || e->trackingspeedattributeid != 0
		|| e->dischargeattributeid != 0 || e->rangeattributeid != 0
		|| e->falloffattributeid != 0 || e->fittingusagechanceattributeid != 0) {
			if(i == n) {
				*out = e->id;
				return DOGMA_OK;
			}

			++i;
		}

		JLN(te, effects, index);
	}

	return DOGMA_NOT_FOUND;
}

int dogma_get_location_effect_attributes(dogma_context_t* ctx, dogma_location_t loc, dogma_effectid_t eid,
                                         double* duration, double* tracking, double* discharge,
                                         double* range, double* falloff, double* usagechance) {
	dogma_env_t* loc_env;
	const dogma_effect_t* e;

	DOGMA_ASSUME_OK(dogma_get_location_env(ctx, loc, &loc_env));
	DOGMA_ASSUME_OK(dogma_get_effect(eid, &e));

	if(e->durationattributeid != 0) {
		DOGMA_ASSUME_OK(dogma_get_env_attribute(ctx, loc_env, e->durationattributeid, duration));
	}
	if(e->trackingspeedattributeid != 0) {
		DOGMA_ASSUME_OK(dogma_get_env_attribute(ctx, loc_env, e->trackingspeedattributeid, tracking));
	}
	if(e->dischargeattributeid != 0) {
		DOGMA_ASSUME_OK(dogma_get_env_attribute(ctx, loc_env, e->dischargeattributeid, discharge));
	}
	if(e->rangeattributeid != 0) {
		DOGMA_ASSUME_OK(dogma_get_env_attribute(ctx, loc_env, e->rangeattributeid, range));
	}
	if(e->falloffattributeid != 0) {
		DOGMA_ASSUME_OK(dogma_get_env_attribute(ctx, loc_env, e->falloffattributeid, falloff));
	}
	if(e->fittingusagechanceattributeid != 0) {
		DOGMA_ASSUME_OK(dogma_get_env_attribute(ctx, loc_env, e->fittingusagechanceattributeid, usagechance));
	}

	return DOGMA_OK;
}
