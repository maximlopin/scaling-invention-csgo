#ifndef DATATYPES_H
#define DATATYPES_H

// World to screen matrix
struct Matrix4x4 {
	float _[16];
};

// 3D Vector
struct Vector3D {
	float x;
	float y;
	float z;
};

// 2D Vector
struct Vector2D {
	float x;
	float y;
};

// Local player
struct Player {
	DWORD baseAddr;
	Vector3D posFeet;
	int health;
	int team;
	Matrix4x4 perspective;
};

struct Entity {
	DWORD baseAddr;
	int health;
	int team;

	ID2D1SolidColorBrush* brush;

	Vector3D posFeet;
	Vector3D posHead;

	Vector2D screenFeet;
	Vector2D screenHead;

	float w;
	bool visible;
};
#endif