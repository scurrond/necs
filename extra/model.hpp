#pragma once 

#include "../necs.hpp"
#include "../ecs.hpp"

using namespace NECS;

// COMPONENTS

struct Health { int value; };
struct Detector { int target; };
struct Position { float x; float y; };
struct Name { std::string value; };

// ARCHETYPES

using A1 = Data<Health>;
using A2 = Data<Health, Position>;
using A3 = Data<Health, Position, Name>;

using Archetypes = Data<A1, A2, A3>;

// EVENTS

struct AEvent { int value; };
struct BEvent { int value; };

using Events = Data<AEvent, BEvent>;

// SINGLETONS

struct S1 {};
struct S2 {};
struct S3 {};

using Singletons = Data<S1, S2, S3>;
