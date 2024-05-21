#pragma once

struct FNarrowPhaseEntityContact;
struct FAngularVelocity;
struct FTransformComponent;
struct FEntityPositionCache;
struct FCollisionSpatialGrid;
struct FVelocity;
struct FPosition;

namespace flecs
{
	struct entity;
}

struct FConservativeAdvancementOutput
{
	float Time { 0.0f };
	bool bInitialOverlap { false };
	bool bCollided { false };
	FVector ContactPoint {};
	FVector ContactNormal {};
};

struct FCollisionOutput
{
	bool bDidOverlap { false };
	float Distance { 0.0f };
	FVector ClosestA {};
	FVector ClosestB {};
	FVector Normal {};
};

struct FSimplex
{
	union
	{
		struct
		{
			FVector A;
			FVector A1;
			FVector A2;

			FVector B;
			FVector B1;
			FVector B2;

			FVector C;
			FVector C1;
			FVector C2;

			FVector D;
			FVector D1;
			FVector D2;
		};
		FVector Points[12];
	};

	int32 Count { 0 };

	FSimplex(): A(), A1(), A2(), B(), B1(), B2(), C(), C1(), C2(), D(), D1(), D2()
	{
	}

	FORCEINLINE void ShufflePoints(const FVector& SupportA, const FVector& SupportB, const FVector& Support)
	{
		for (int i = 8; i >= 0; --i)
		{
			Points[i + 3] = Points[i];
		}

		A  = Support;
		A1 = SupportA;
		A2 = SupportB;
	}
};

struct FCollisionHelper
{
	static FTransform LerpTransform(const FTransform& Start, const FTransform& End, float Alpha);

	static float ApproximateDistanceFromOrigin(const TArray<FVector>& Simplex);

	static FVector Support(const FCollisionShape& Shape, const FTransform& Transform, const FVector& Direction);

	FORCEINLINE static FVector Support(const FCollisionShape& ShapeA, const FTransform& TransformA, const FCollisionShape& ShapeB,
	                                   const FTransform& TransformB, const FVector& Direction)
	{
		const FVector NormalizedDirection = Direction.GetSafeNormal();
		return Support(ShapeA, TransformA, NormalizedDirection) - Support(ShapeB, TransformB, -NormalizedDirection);
	}

	static float GetBoundingRadius(const FCollisionShape& Shape, const FTransform& Transform)
	{
		switch(Shape.ShapeType)
		{
		case ECollisionShape::Box:
			{
				const FVector Extents = Shape.GetExtent();
				const FVector ScaledExtents = Extents * Transform.GetScale3D();
				return ScaledExtents.Size();
			}
		case ECollisionShape::Sphere:
			return Shape.GetSphereRadius() * Transform.GetMaximumAxisScale();
		case ECollisionShape::Capsule:
			return FMath::Max(Shape.GetCapsuleRadius(), Shape.GetCapsuleHalfHeight()) * Transform.GetMaximumAxisScale();
		default:
			return 0.0f;
		}
	}

	FORCEINLINE static bool SameDirection(const FVector& A, const FVector& B)
	{
		return A.Dot(B) > 0;
	}

	static FVector CalculateLineNormal(const FVector& A, const FVector& B);

	static FVector CalculateTriangleNormal(const FVector& A, const FVector& B, const FVector& C);

	static FVector CalculateTetrahedronNormal(const FVector& A, const FVector& B, const FVector& C, const FVector& D);

	static FVector CalculateNormal(const TArray<FVector>& Simplex);

	static void HandleLine(TArray<FVector>& Simplex, FVector& Direction);

	static void HandleTriangle(TArray<FVector>& Simplex, FVector& Direction);

	static bool HandleTetrahedron(TArray<FVector>& Simplex, FVector& Direction);

	static bool UpdateSimplex(TArray<FVector>& Simplex, FVector& Direction);

	static FVector SolveSimplex2(FSimplex& Simplex, FVector& OutDirection);

	static FVector SolveSimplex3(FSimplex& Simplex, FVector& OutDirection);

	static FVector SolveSimplex4(FSimplex& Simplex, FVector& OutDirection);

	FORCEINLINE static float TriangleArea2D(const float X1, const float Y1, const float X2, const float Y2, const float X3, const float Y3);

	static FVector ConvertBarycentricCoordsToCartesianCoordsTriangle(const FVector& A, const FVector& B, const FVector& C, const FVector& Barycentric);

	static FVector ConvertBarycentricCoordsToCartesianCoordsLine(const FVector& A, const FVector& B, const float Barycentric);

	static FVector GetBarycentricCoordsTriangle(const FVector& A, const FVector& B, const FVector& C, const FVector& P);

	static float GetBarycentricScalarLine(const FVector& A, const FVector& B, const FVector& P);

	static TTuple<FVector, FVector> GetLocalPoints(FSimplex Simplex, const FVector& Point);

	static FCollisionOutput GJK_Complex(const FCollisionShape& ShapeA, const FTransform& TransformA, const FCollisionShape& ShapeB, const FTransform& TransformB);

	static FCollisionOutput GJK_Simple(const FCollisionShape& ShapeA, const FTransform& TransformA, const FCollisionShape& ShapeB, const FTransform& TransformB);

	static FConservativeAdvancementOutput ConservativeAdvancement(const FCollisionShape& ShapeA,
	                                                              const FTransform& TransformFromA,
	                                                              const FTransform& TransformToA,
	                                                              const FCollisionShape& ShapeB,
	                                                              const FTransform& TransformFromB,
	                                                              const FTransform& TransformToB);

	static void BoxBroadphase(float DeltaTime, const flecs::entity& Entity, const FCollisionShape& CollisionShape, const FPosition& Position, const FVelocity& Velocity, FCollisionSpatialGrid& CollisionSpatialGrid, TArray<FEntityPositionCache>& CollisionCandidates);
	static bool NarrowPhase(const float DeltaTime, const flecs::entity& Entity, const FCollisionShape& CollisionShape,
	                        const FTransformComponent& Transform, const FVector& Velocity, const FAngularVelocity& AngularVelocity,
	                        const TArray<FEntityPositionCache>& CollisionCandidates, FPosition& Position, TSet<FNarrowPhaseEntityContact>& NarrowPhaseEntityContacts, float& OutTime);
};
