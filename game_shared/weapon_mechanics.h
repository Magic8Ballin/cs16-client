#ifndef GLOVEWORKS_WEAPON_MECHANICS_H
#define GLOVEWORKS_WEAPON_MECHANICS_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace gw
{
const int kMaxRecoilPoints = 64;

struct RecoilPoint
{
	float vertical;
	float horizontal;
};

struct WeaponMechanicsConfig
{
	bool enabled;
	float baseSpread;
	float standInaccuracy;
	float crouchInaccuracy;
	float moveInaccuracy;
	float airInaccuracy;
	float ladderInaccuracy;
	float moveStartFraction;
	float moveFullFraction;
	float moveExponent;
	float fireImpulse;
	float recoveryStandEarly;
	float recoveryStandFinal;
	float recoveryCrouchEarly;
	float recoveryCrouchFinal;
	float recoveryTransitionStart;
	float recoveryTransitionEnd;
	float maxFirePenalty;
	float cycleTime;
	float decayDelayCycles;
	float indexDecayRate;
	float viewScale;
	float varianceScale;
	bool wrapPattern;
	int patternCount;
	RecoilPoint pattern[kMaxRecoilPoints];
};

struct WeaponMechanicsState
{
	float firePenalty;
	float recoilIndex;
	float lastShotTime;
};

struct ShotOffset
{
	float x;
	float y;
};

inline WeaponMechanicsConfig Defaults(bool ak47)
{
	WeaponMechanicsConfig c;
	memset(&c, 0, sizeof(c));
	c.enabled = true;
	c.baseSpread = ak47 ? 0.0040f : 0.0030f;
	c.standInaccuracy = ak47 ? 0.0070f : 0.0085f;
	c.crouchInaccuracy = ak47 ? 0.0045f : 0.0055f;
	c.moveInaccuracy = ak47 ? 0.0750f : 0.0950f;
	c.airInaccuracy = ak47 ? 0.2900f : 0.3600f;
	c.ladderInaccuracy = ak47 ? 0.1200f : 0.1500f;
	c.moveStartFraction = ak47 ? 0.34f : 0.30f;
	c.moveFullFraction = 0.95f;
	c.moveExponent = 2.0f;
	c.fireImpulse = ak47 ? 0.0115f : 0.0850f;
	c.recoveryStandEarly = ak47 ? 0.37f : 0.48f;
	c.recoveryStandFinal = ak47 ? 0.50f : 0.62f;
	c.recoveryCrouchEarly = ak47 ? 0.30f : 0.38f;
	c.recoveryCrouchFinal = ak47 ? 0.40f : 0.50f;
	c.recoveryTransitionStart = ak47 ? 3.0f : 1.0f;
	c.recoveryTransitionEnd = ak47 ? 8.0f : 4.0f;
	c.maxFirePenalty = ak47 ? 0.105f : 0.190f;
	c.cycleTime = ak47 ? 0.0955f : 0.225f;
	c.decayDelayCycles = ak47 ? 2.0f : 1.5f;
	c.indexDecayRate = ak47 ? 5.0f : 4.0f;
	c.viewScale = 1.0f;
	c.wrapPattern = !ak47;
	static const RecoilPoint akPattern[] = {
		{1.70f,0.00f},{1.82f,0.10f},{1.94f,-0.12f},{2.04f,0.20f},
		{2.12f,-0.24f},{2.18f,0.32f},{2.24f,0.42f},{2.28f,-0.48f},
		{2.32f,-0.58f},{2.36f,0.62f},{2.40f,0.72f},{2.42f,-0.76f},
		{2.44f,-0.82f},{2.46f,0.80f},{2.46f,0.68f},{2.46f,-0.62f}
	};
	static const RecoilPoint deaglePattern[] = {
		{5.20f,0.18f},{5.55f,-0.30f},{5.80f,0.38f},{6.00f,-0.22f}
	};
	const RecoilPoint *points = ak47 ? akPattern : deaglePattern;
	c.patternCount = ak47 ? 16 : 4;
	for (int i = 0; i < c.patternCount; ++i) c.pattern[i] = points[i];
	return c;
}

inline float Clamp(float value, float low, float high)
{
	return value < low ? low : (value > high ? high : value);
}

inline float Lerp(float a, float b, float t)
{
	return a + (b - a) * Clamp(t, 0.0f, 1.0f);
}

inline unsigned int Hash(unsigned int value)
{
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	return value ^ (value >> 16);
}

inline float UnitRandom(int seed, int offset)
{
	return (Hash((unsigned int)seed ^ (0x9e3779b9u * (unsigned int)(offset + 1))) & 0x00ffffffu) / 16777216.0f;
}

inline ShotOffset ComputeShotOffset(int seed, float inaccuracy, float spread)
{
	const float tau = 6.28318530717958647692f;
	const float inaccuracyRadius = sqrtf(UnitRandom(seed, 0)) * inaccuracy;
	const float inaccuracyAngle = UnitRandom(seed, 1) * tau;
	const float spreadRadius = sqrtf(UnitRandom(seed, 2)) * spread;
	const float spreadAngle = UnitRandom(seed, 3) * tau;
	ShotOffset result;
	result.x = cosf(inaccuracyAngle) * inaccuracyRadius + cosf(spreadAngle) * spreadRadius;
	result.y = sinf(inaccuracyAngle) * inaccuracyRadius + sinf(spreadAngle) * spreadRadius;
	return result;
}

inline float MovementPenalty(const WeaponMechanicsConfig &config, float speed, float maxSpeed)
{
	if (maxSpeed <= 0.0f)
		return 0.0f;
	const float fraction = speed / maxSpeed;
	const float width = config.moveFullFraction - config.moveStartFraction;
	if (width <= 0.0f)
		return fraction >= config.moveFullFraction ? config.moveInaccuracy : 0.0f;
	const float t = Clamp((fraction - config.moveStartFraction) / width, 0.0f, 1.0f);
	return config.moveInaccuracy * powf(t, config.moveExponent);
}

inline float RecoveryTime(const WeaponMechanicsConfig &config, bool crouched, float recoilIndex)
{
	const float width = config.recoveryTransitionEnd - config.recoveryTransitionStart;
	const float t = width > 0.0f ? (recoilIndex - config.recoveryTransitionStart) / width : 1.0f;
	return crouched
		? Lerp(config.recoveryCrouchEarly, config.recoveryCrouchFinal, t)
		: Lerp(config.recoveryStandEarly, config.recoveryStandFinal, t);
}

inline void UpdateState(const WeaponMechanicsConfig &config, WeaponMechanicsState &state, float now, bool crouched)
{
	if (state.lastShotTime <= 0.0f || now <= state.lastShotTime)
		return;
	const float elapsed = now - state.lastShotTime;
	const float recovery = RecoveryTime(config, crouched, state.recoilIndex);
	if (recovery > 0.0f)
		state.firePenalty *= expf((-2.302585092994046f / recovery) * elapsed);
	const float delay = config.cycleTime * config.decayDelayCycles;
	if (elapsed > delay)
		state.recoilIndex *= expf(-config.indexDecayRate * (elapsed - delay));
	if (state.firePenalty < 0.000001f)
		state.firePenalty = 0.0f;
	if (state.recoilIndex < 0.000001f)
		state.recoilIndex = 0.0f;
}

inline float ComputeInaccuracy(const WeaponMechanicsConfig &config, const WeaponMechanicsState &state,
	float speed, float maxSpeed, bool crouched, bool onGround, bool onLadder)
{
	float result = crouched ? config.crouchInaccuracy : config.standInaccuracy;
	result += MovementPenalty(config, speed, maxSpeed);
	if (!onGround)
		result += config.airInaccuracy;
	if (onLadder)
		result += config.ladderInaccuracy;
	return result + state.firePenalty;
}

inline RecoilPoint GetRecoil(const WeaponMechanicsConfig &config, float recoilIndex)
{
	RecoilPoint result = { 0.0f, 0.0f };
	if (config.patternCount <= 0)
		return result;
	int index = (int)recoilIndex;
	if (config.wrapPattern)
		index %= config.patternCount;
	else if (index >= config.patternCount)
		index = config.patternCount - 1;
	result = config.pattern[index];
	return result;
}

inline void CommitShot(const WeaponMechanicsConfig &config, WeaponMechanicsState &state, float now)
{
	state.firePenalty = Clamp(state.firePenalty + config.fireImpulse, 0.0f, config.maxFirePenalty);
	state.recoilIndex += 1.0f;
	state.lastShotTime = now;
}

inline const char *FindValue(const char *json, const char *key)
{
	char pattern[96];
	pattern[0] = '"';
	strncpy(pattern + 1, key, sizeof(pattern) - 3);
	pattern[sizeof(pattern) - 2] = '\0';
	strcat(pattern, "\"");
	const char *position = strstr(json, pattern);
	if (!position)
		return 0;
	position = strchr(position + strlen(pattern), ':');
	return position ? position + 1 : 0;
}

inline bool ReadFloat(const char *json, const char *key, float &value)
{
	const char *position = FindValue(json, key);
	if (!position)
		return false;
	char *end = 0;
	const double parsed = strtod(position, &end);
	if (end == position || parsed != parsed)
		return false;
	value = (float)parsed;
	return true;
}

inline bool ReadBool(const char *json, const char *key, bool &value)
{
	const char *position = FindValue(json, key);
	if (!position)
		return false;
	while (*position == ' ' || *position == '\t' || *position == '\r' || *position == '\n') ++position;
	if (!strncmp(position, "true", 4)) { value = true; return true; }
	if (!strncmp(position, "false", 5)) { value = false; return true; }
	return false;
}

inline bool ParsePattern(const char *json, WeaponMechanicsConfig &config)
{
	const char *position = FindValue(json, "pattern");
	if (!position || !(position = strchr(position, '[')))
		return false;
	config.patternCount = 0;
	while (config.patternCount < kMaxRecoilPoints)
	{
		const char *object = strchr(position, '{');
		const char *arrayEnd = strchr(position, ']');
		if (!object || (arrayEnd && arrayEnd < object))
			break;
		const char *objectEnd = strchr(object, '}');
		if (!objectEnd)
			return false;
		const char *vertical = FindValue(object, "vertical");
		const char *horizontal = FindValue(object, "horizontal");
		if (!vertical || !horizontal || vertical > objectEnd || horizontal > objectEnd)
			return false;
		config.pattern[config.patternCount].vertical = (float)strtod(vertical, 0);
		config.pattern[config.patternCount].horizontal = (float)strtod(horizontal, 0);
		++config.patternCount;
		position = objectEnd + 1;
	}
	return config.patternCount > 0;
}

inline bool ParseConfig(const char *json, WeaponMechanicsConfig &config)
{
	if (!json || !ReadBool(json, "enabled", config.enabled)) return false;
	const char *keys[] = { "baseSpread", "standInaccuracy", "crouchInaccuracy", "moveInaccuracy",
		"airInaccuracy", "ladderInaccuracy", "moveStartFraction", "moveFullFraction", "moveExponent",
		"fireImpulse", "recoveryStandEarly", "recoveryStandFinal", "recoveryCrouchEarly",
		"recoveryCrouchFinal", "recoveryTransitionStart", "recoveryTransitionEnd", "maxFirePenalty",
		"cycleTime", "decayDelayCycles", "indexDecayRate", "viewScale", "varianceScale" };
	float *values[] = { &config.baseSpread, &config.standInaccuracy, &config.crouchInaccuracy, &config.moveInaccuracy,
		&config.airInaccuracy, &config.ladderInaccuracy, &config.moveStartFraction, &config.moveFullFraction,
		&config.moveExponent, &config.fireImpulse, &config.recoveryStandEarly, &config.recoveryStandFinal,
		&config.recoveryCrouchEarly, &config.recoveryCrouchFinal, &config.recoveryTransitionStart,
		&config.recoveryTransitionEnd, &config.maxFirePenalty, &config.cycleTime, &config.decayDelayCycles,
		&config.indexDecayRate, &config.viewScale, &config.varianceScale };
	for (unsigned int i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
		if (!ReadFloat(json, keys[i], *values[i])) return false;
	const char *endPolicy = FindValue(json, "endPolicy");
	config.wrapPattern = endPolicy && strstr(endPolicy, "wrap") && (!strchr(endPolicy, ',') || strstr(endPolicy, "wrap") < strchr(endPolicy, ','));
	if (!ParsePattern(json, config)) return false;
	return config.baseSpread >= 0.0f && config.standInaccuracy >= 0.0f && config.crouchInaccuracy >= 0.0f
		&& config.moveInaccuracy >= 0.0f && config.airInaccuracy >= 0.0f && config.ladderInaccuracy >= 0.0f
		&& config.moveStartFraction >= 0.0f && config.moveFullFraction > config.moveStartFraction
		&& config.moveFullFraction <= 1.0f && config.moveExponent > 0.0f && config.fireImpulse >= 0.0f
		&& config.recoveryStandEarly > 0.0f && config.recoveryStandFinal > 0.0f
		&& config.recoveryCrouchEarly > 0.0f && config.recoveryCrouchFinal > 0.0f
		&& config.recoveryTransitionEnd >= config.recoveryTransitionStart && config.maxFirePenalty >= 0.0f
		&& config.cycleTime > 0.0f && config.decayDelayCycles >= 0.0f && config.indexDecayRate >= 0.0f;
}
}

#endif
