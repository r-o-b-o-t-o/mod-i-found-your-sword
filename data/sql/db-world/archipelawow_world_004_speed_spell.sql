-- Serverside-only spell backing the "Progressive Movement Speed" Archipelago item.
--
-- The three effects cover the three ways a character moves: on foot, on a ground mount and on a
-- flying mount. All three are of the "always" kind (129, 130, 209), which Unit::UpdateSpeed folds
-- in as a multiplier over the speed it has already worked out, so the bonus stacks on top of mount
-- speed instead of losing to it the way "increase speed" auras would.
--
-- The spell is passive (Attributes 448 = passive | hidden in UI | hidden in combat log), so the
-- aura is never sent to the client and the client never has to know an id its Spell.dbc lacks.
-- The id sits in the 100000+ range AzerothCore keeps for its own serverside spells.

SET @apSpeedSpellId := 100500;

DELETE FROM `spell_dbc` WHERE `ID` = @apSpeedSpellId;
INSERT INTO `spell_dbc` (
    `ID`, `Attributes`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`, `EquippedItemClass`, `SchoolMask`,
    `Effect_1`, `EffectAura_1`, `ImplicitTargetA_1`,
    `Effect_2`, `EffectAura_2`, `ImplicitTargetA_2`,
    `Effect_3`, `EffectAura_3`, `ImplicitTargetA_3`,
    `Name_Lang_enUS`
) VALUES (
    @apSpeedSpellId, 448, 1, 21, 1, -1, 1,
    6, 129, 1, -- SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_SPEED_ALWAYS, TARGET_UNIT_CASTER
    6, 130, 1, -- SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS, TARGET_UNIT_CASTER
    6, 209, 1, -- SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_MOUNTED_FLIGHT_SPEED_ALWAYS, TARGET_UNIT_CASTER
    'Archipelago Movement Speed - serverside spell'
);
