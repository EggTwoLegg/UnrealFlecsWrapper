#include "..\..\Public\UECS\CollisionHelper.h"

#include "UECS/Components/AngularVelocity.h"
#include "UECS/Components/BaseComponents.h"
#include "UECS/Components/PhysicsAndCollision/CollisionSpatialGrid.h"
#include "UECS/Components/PhysicsAndCollision/ICollisionHandler.h"
#include "UECS/Components/PhysicsAndCollision/NarrowPhaseCollisionCandidates.h"
#include "UECS/Components/PhysicsAndCollision/NarrowPhaseEntityContacts.h"
#include "UECS/Components/PhysicsAndCollision/OverlapCollision.h"

FTransform FCollisionHelper::LerpTransform(const FTransform& Start, const FTransform& End, float Alpha)
{
	FTransform Result;
		
	Result.SetTranslation(FMath::Lerp(Start.GetTranslation(), End.GetTranslation(), Alpha));
	Result.SetRotation(FQuat::Slerp(Start.GetRotation(), End.GetRotation(), Alpha));
	Result.SetScale3D(FMath::Lerp(Start.GetScale3D(), End.GetScale3D(), Alpha));

	return Result;
}

float FCollisionHelper::ApproximateDistanceFromOrigin(const TArray<FVector>& Simplex)
{
	float MinDistance = std::numeric_limits<float>::infinity();

	for (const auto& Point : Simplex)
	{
		const float Distance = Point.Length();
		if (Distance < MinDistance)
		{
			MinDistance = Distance;
		}
	}
		
	return MinDistance;
}

FVector FCollisionHelper::Support(const FCollisionShape& Shape, const FTransform& Transform, const FVector& Direction)
{
	switch (Shape.ShapeType)
	{
	case ECollisionShape::Box:
		{
			const FVector X = Transform.GetScaledAxis(EAxis::X);
			const FVector Y = Transform.GetScaledAxis(EAxis::Y);
			const FVector Z = Transform.GetScaledAxis(EAxis::Z);

			const FVector HalfExtents = Shape.GetExtent() * Transform.GetScale3D();

			return (X * FMath::Sign(X.Dot(Direction)) * HalfExtents.X)
				+ (Y * FMath::Sign(Y.Dot(Direction)) * HalfExtents.Y)
				+ (Z * FMath::Sign(Z.Dot(Direction)) * HalfExtents.Z)
				+ Transform.GetTranslation();
		}

	case ECollisionShape::Sphere:
		return Direction * Shape.GetSphereRadius() * Transform.GetMaximumAxisScale() + Transform.GetTranslation();
			
	case ECollisionShape::Capsule:
		{
			const FVector CapsuleAxis = Transform.GetScaledAxis(EAxis::Z);
			const float HalfLength = Shape.GetCapsuleHalfHeight();
			const float Radius = Shape.GetCapsuleRadius();

			return Transform.GetTranslation()
				+ CapsuleAxis * (HalfLength * FMath::Sign(CapsuleAxis.Dot(Direction)))
				+ Direction.GetSafeNormal()
				* Radius;
		}
			
	default:
		return FVector::ZeroVector;
	}
}

FVector FCollisionHelper::CalculateLineNormal(const FVector& A, const FVector& B)
{
	const FVector AB = B - A;
	const FVector AO = -A;
		
	float t = -AO.Dot(AB) / AB.SizeSquared();
	t = FMath::Clamp(t, 0.0f, 1.0f); // Clamping t to stay within the segment

	const FVector ClosestPoint = A + t * AB;
	const FVector Normal = -ClosestPoint;
		
	return Normal.GetSafeNormal();
}

FVector FCollisionHelper::CalculateTriangleNormal(const FVector& A, const FVector& B, const FVector& C)
{
	const FVector AB = B - A;
	const FVector AC = C - A;

	FVector Normal = AB.Cross(AC);

	const FVector AO = -A;

	if (Normal.Dot(AO) < 0) { Normal = -Normal; }
		
	return Normal.GetSafeNormal();
}

FVector FCollisionHelper::CalculateTetrahedronNormal(const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{
	// Calculate normals for each face
	const FVector ABC = (B - A).Cross(C - A);
	const FVector ACD = (C - A).Cross(D - A);
	const FVector ADB = (D - A).Cross(B - A);
	const FVector BDC = (D - B).Cross(C - B);

	const FVector AO = -A;

	// Find the face whose normal points towards the origin.
	FVector Normal;
	if (ABC.Dot(AO) > 0) { Normal = ABC; }
	if (ACD.Dot(AO) > 0) { Normal = ACD; }
	if (ADB.Dot(AO) > 0) { Normal = ADB; } 
	if (BDC.Dot((B - D)) > 0) { Normal = BDC; } // Using B - D since origin is relative to B here.

	return Normal.GetSafeNormal();
}

FVector FCollisionHelper::CalculateNormal(const TArray<FVector>& Simplex)
{
	switch (Simplex.Num())
	{
	case 2:
		return CalculateLineNormal(Simplex[0], Simplex[1]);
	case 3:
		return CalculateTriangleNormal(Simplex[0], Simplex[1], Simplex[2]);
	case 4:
		return CalculateTetrahedronNormal(Simplex[0], Simplex[1], Simplex[2], Simplex[3]);
	default:
		return FVector::ZeroVector;
	}
}

void FCollisionHelper::HandleLine(TArray<FVector>& Simplex, FVector& Direction)
{
	const FVector& A = Simplex[0];
	const FVector& B = Simplex[1];
	const FVector AB = B - A;
	const FVector AO = -A;
		
	if (SameDirection(AB, AO))
	{
		Direction = AB.Cross(AO).Cross(AB);
	}
	else
	{
		Simplex = { A }; // Remove b, keep a.
		Direction = AO;
	}
}

void FCollisionHelper::HandleTriangle(TArray<FVector>& Simplex, FVector& Direction)
{
	const FVector& A = Simplex[0];
	const FVector& B = Simplex[1];
	const FVector& C = Simplex[2];
	const FVector AB = B - A;
	const FVector AC = C - A;
	const FVector AO = -A;
		
	const FVector ABC = AB.Cross(AC);

	if (SameDirection(ABC.Cross(AC), AO)) {
		if (SameDirection(AC, AO)) {
			Simplex = { A, C };
			Direction = (AC.Cross(AO).Cross(AC));
		}
		else
		{
			return HandleLine(Simplex = { A, B }, Direction);
		}
	}
 
	else {
		if (SameDirection(AB.Cross(ABC), AO))
		{
			return HandleLine(Simplex = { A, B }, Direction);
		}
			
		if (SameDirection(ABC, AO))
		{
			Direction = ABC;
		}
		else
		{
			Simplex = { A, C, B };
			Direction = -ABC;
		}
	}
}

bool FCollisionHelper::HandleTetrahedron(TArray<FVector>& Simplex, FVector& Direction)
{
	const FVector& A = Simplex[0];
	const FVector& B = Simplex[1];
	const FVector& C = Simplex[2];
	const FVector& D = Simplex[3];
	const FVector AO = -A;

	const FVector ABC = (B - A).Cross(C - A);
	const FVector ACD = (C - A).Cross(D - A);
	const FVector ADB = (D - A).Cross(B - A);

	if (SameDirection(ABC, AO))
	{
		Simplex.RemoveAt(3); // Remove D
		Direction = ABC;
		HandleTriangle(Simplex, Direction);
		return false;
	}

	if (SameDirection(ACD, AO))
	{
		Simplex.RemoveAt(1); // Remove B
		Direction = ACD;
		HandleTriangle(Simplex, Direction);
		return false;
	}

	if (SameDirection(ADB, AO))
	{
		Simplex = {A, D, B}; // Reorder to keep correct orientation
		Direction = ADB;
		HandleTriangle(Simplex, Direction);
		return false;
	}

	// If we reach here, the origin is inside the tetrahedron, so there is a collision.
	return true;
}

bool FCollisionHelper::UpdateSimplex(TArray<FVector>& Simplex, FVector& Direction)
{
	check(!Simplex.IsEmpty());

	switch (Simplex.Num())
	{
	case 2:
		HandleLine(Simplex, Direction);
		return false;
	case 3:
		HandleTriangle(Simplex, Direction);
		return false;
	case 4:
		return HandleTetrahedron(Simplex, Direction);
	default:
		return false;
	}
}

FVector FCollisionHelper::SolveSimplex2(FSimplex& Simplex, FVector& OutDirection)
{
	const FVector Ab = Simplex.B - Simplex.A;
	const FVector Ao = -Simplex.A;

	if (SameDirection(Ab, Ao))
	{
		// The origin falls on the line.
		// Project the origin onto the line and return.
		const float T = FVector::DotProduct(Ao, Ab) / FVector::DotProduct(Ab, Ab);
		OutDirection = FVector::CrossProduct(FVector::CrossProduct(Ab, Ao), Ab);
		return Simplex.A + T * Ab;
	}
		
	// The origin is closer to A.
	Simplex.Count = 1;
	OutDirection = Ao;
	return Simplex.A;
}

FVector FCollisionHelper::SolveSimplex3(FSimplex& Simplex, FVector& OutDirection)
{
	const FVector Abc = FVector::CrossProduct(Simplex.B - Simplex.A, Simplex.C - Simplex.A);
	const FVector Ac = Simplex.C - Simplex.A;
	const FVector Ao = -Simplex.A;

	if (SameDirection(FVector::CrossProduct(Abc, Ac), Ao))
	{
		if (SameDirection(Ac, Ao))
		{
			// The origin is nearest to the line AC
			Simplex.B = Simplex.C;
			Simplex.Count = 2;
			const float T = FVector::DotProduct(Ao, Ac) / FVector::DotProduct(Ac, Ac);
			OutDirection = FVector::CrossProduct(FVector::CrossProduct(Ac, Ao), Ac);
			return Simplex.A + T * Ac;
		}

		const FVector Ab = Simplex.B - Simplex.A;
		if (SameDirection(Ab, Ao))
		{
			// The origin is nearest to the line AB
			Simplex.Count = 2;
			const float T = FVector::DotProduct(Ao, Ab) / FVector::DotProduct(Ab, Ab);
			OutDirection = FVector::CrossProduct(FVector::CrossProduct(Ab, Ao), Ab);
			return Simplex.A + T * Ab;
		}
	        	
		// The origin is nearest to the point A
		Simplex.Count = 1;
		OutDirection = Ao;
		return Simplex.A;
	}

	const FVector Ab = Simplex.B - Simplex.A;
	if (SameDirection(FVector::CrossProduct(Ab, Abc), Ao))
	{
		if (SameDirection(Ab, Ao))
		{
			// The origin is nearest to the line AB
			Simplex.Count = 2;
			const float T = FVector::DotProduct(Ao, Ab) / FVector::DotProduct(Ab, Ab);
			OutDirection = FVector::CrossProduct(FVector::CrossProduct(Ab, Ao), Ab);
			return Simplex.A + T * Ab;
		}
	    	
		// The origin is nearest to the point A
		Simplex.Count = 1;
		OutDirection = Ao;
		return Simplex.A;
	}
		
	if (SameDirection(Abc, Ao))
	{
		// The origin is nearest to the triangle ABC
		const FVector Bo = -Simplex.B;
		const FVector Co = -Simplex.C;
		const float D1 = FVector::DotProduct(Ab, Ao);
		const float D2 = FVector::DotProduct(Ac, Ao);
		const float D3 = FVector::DotProduct(Ab, Bo);
		const float D4 = FVector::DotProduct(Ac, Bo);
		const float D5 = FVector::DotProduct(Ab, Co);
		const float D6 = FVector::DotProduct(Ac, Co);
		const float Va = D3 * D6 - D5 * D4;
		const float Vb = D5 * D2 - D1 * D6;
		const float Vc = D1 * D4 - D3 * D2;
		const float Denom = 1.0f / (Va + Vb + Vc);
		const float V = Vb * Denom;
		const float W = Vc * Denom;
		OutDirection = Abc;
		return Simplex.A + V * Ab + W * Ac;
	}
		
	// The origin is nearest to the triangle ACB
	Swap(Simplex.B, Simplex.C);
	Swap(Simplex.B1, Simplex.C1);
	Swap(Simplex.B2, Simplex.C2);
		
	const FVector Bo = -Simplex.B;
	const FVector Co = -Simplex.C;
	const float D1 = FVector::DotProduct(Ac, Ao);
	const float D2 = FVector::DotProduct(Ab, Ao);
	const float D3 = FVector::DotProduct(Ac, Co);
	const float D4 = FVector::DotProduct(Ab, Co);
	const float D5 = FVector::DotProduct(Ac, Bo);
	const float D6 = FVector::DotProduct(Ab, Bo);
	const float Va = D3 * D6 - D5 * D4;
	const float Vb = D5 * D2 - D1 * D6;
	const float Vc = D1 * D4 - D3 * D2;
	const float Denom = 1.0f / (Va + Vb + Vc);
	const float V = Vb * Denom;
	const float W = Vc * Denom;
	OutDirection = -Abc;
	return Simplex.A + V * Ab + W * Ac;
}

FVector FCollisionHelper::SolveSimplex4(FSimplex& Simplex, FVector& OutDirection)
{
	const FVector Abc = FVector::CrossProduct(Simplex.B - Simplex.A, Simplex.C - Simplex.A);
	const FVector Acd = FVector::CrossProduct(Simplex.C - Simplex.A, Simplex.D - Simplex.A);
	const FVector Adb = FVector::CrossProduct(Simplex.D - Simplex.A, Simplex.B - Simplex.A);
	const FVector Ao = -Simplex.A;

	const bool bSameAbcDir = SameDirection(Abc, Ao);
	const bool bSameAcdDir = SameDirection(Acd, Ao);
	const bool bSameAdbDir = SameDirection(Adb, Ao);

	if (!bSameAbcDir && !bSameAcdDir && !bSameAdbDir)
	{
		OutDirection = FVector(0, 0, 0);
		return FVector(0, 0, 0);
	}

	if (bSameAbcDir && !bSameAcdDir && !bSameAdbDir)
	{
		Simplex.Count = 3;
		return SolveSimplex3(Simplex, OutDirection);
	}

	if (!bSameAbcDir && bSameAcdDir && !bSameAdbDir)
	{
		Simplex.B = Simplex.C;
		Simplex.C = Simplex.D;
		Simplex.Count = 3;
		return SolveSimplex3(Simplex, OutDirection);
	}

	if (!bSameAbcDir && !bSameAcdDir && bSameAdbDir)
	{
		Simplex.C = Simplex.B;
		Simplex.B = Simplex.D;
		Simplex.Count = 3;
		return SolveSimplex3(Simplex, OutDirection);
	}

	// The origin potentially falls on multiple triangles.
	FSimplex SimplexAbc = Simplex;
	SimplexAbc.Count = 3;

	FSimplex SimplexAcd = Simplex;
	SimplexAcd.B = SimplexAcd.C;
	SimplexAcd.C = SimplexAcd.D;
	SimplexAcd.Count = 3;

	FSimplex SimplexAdb = Simplex;
	SimplexAdb.C = SimplexAdb.B;
	SimplexAdb.B = SimplexAdb.D;
	SimplexAdb.Count = 3;

	FVector DirAbc, DirAcd, DirAdb;

	const FVector PAbc = SolveSimplex3(SimplexAbc, DirAbc);
	const FVector PAcd = SolveSimplex3(SimplexAcd, DirAcd);
	const FVector PAdb = SolveSimplex3(SimplexAdb, DirAdb);

	const float AbcD2 = FVector::DotProduct(PAbc, PAbc);
	const float AcdD2 = FVector::DotProduct(PAcd, PAcd);
	const float AdbD2 = FVector::DotProduct(PAdb, PAdb);

	if (AbcD2 <= AcdD2 && AbcD2 <= AdbD2)
	{
		Simplex = SimplexAbc;
		OutDirection = DirAbc;
		return PAbc;
	}
		
	if (AcdD2 <= AbcD2 && AcdD2 <= AdbD2)
	{
		Simplex = SimplexAcd;
		OutDirection = DirAcd;
		return PAcd;
	}
		
	if (AdbD2 <= AbcD2 && AdbD2 <= AcdD2)
	{
		Simplex = SimplexAdb;
		OutDirection = DirAdb;
		return PAdb;
	}

	// Should never be reached...
	check(false);
		
	OutDirection = FVector(0, 0, 0);
	return FVector(0, 0, 0);
}

float FCollisionHelper::TriangleArea2D(const float X1, const float Y1, const float X2, const float Y2, const float X3, const float Y3)
{
	return (X1 - X2) * (Y2 - Y3) - (X2 - X3) * (Y1 - Y2);
}

FVector FCollisionHelper::ConvertBarycentricCoordsToCartesianCoordsTriangle(const FVector& A, const FVector& B, const FVector& C, const FVector& Barycentric)
{
	FVector Result;
		
	Result.X = Barycentric.X * A.X + Barycentric.Y * B.X + Barycentric.Z * C.X;
	Result.Y = Barycentric.X * A.Y + Barycentric.Y * B.Y + Barycentric.Z * C.Y;
	Result.Z = Barycentric.X * A.Z + Barycentric.Y * B.Z + Barycentric.Z * C.Z;

	return Result;
}

FVector FCollisionHelper::ConvertBarycentricCoordsToCartesianCoordsLine(const FVector& A, const FVector& B, const float Barycentric)
{
	FVector Result;

	Result.X = A.X + Barycentric * (B.X - A.X);
	Result.Y = A.Y + Barycentric * (B.Y - A.Y);
	Result.Z = A.Z + Barycentric * (B.Z - A.Z);

	return Result;
}

FVector FCollisionHelper::GetBarycentricCoordsTriangle(const FVector& A, const FVector& B, const FVector& C, const FVector& P)
{
	const FVector Abc = FVector::CrossProduct(B - A, C - A);

	float Nu, Nv, Ood;

	const float X = FMath::Abs(Abc.X);
	const float Y = FMath::Abs(Abc.Y);
	const float Z = FMath::Abs(Abc.Z);

	if (X >= Y && X >= Z)
	{
		// X is the largest, so project onto the YZ plane.
		Nu = TriangleArea2D(P.Y, P.Z, B.Y, B.Z, C.Y, C.Z);
		Nv = TriangleArea2D(P.Y, P.Z, C.Y, C.Z, A.Y, A.Z);
		Ood = 1.0f / Abc.X;
	}
	else if (Y >= X && Y >= Z)
	{
		// Y is the largest, so project onto the XZ plane.
		Nu = TriangleArea2D(P.X, P.Z, B.X, B.Z, C.X, C.Z);
		Nv = TriangleArea2D(P.X, P.Z, C.X, C.Z, A.X, A.Z);
		Ood = 1.0f / -Abc.Y; // Negated because of the coordinate system orientation
	}
	else
	{
		// Z is the largest, so project onto the XY plane.
		Nu = TriangleArea2D(P.X, P.Y, B.X, B.Y, C.X, C.Y);
		Nv = TriangleArea2D(P.X, P.Y, C.X, C.Y, A.X, A.Y);
		Ood = 1.0f / Abc.Z;
	}

	FVector Result;
	Result.X = Nu * Ood;
	Result.Y = Nv * Ood;
	Result.Z = 1.0f - Result.X - Result.Y;

	return Result;
}

float FCollisionHelper::GetBarycentricScalarLine(const FVector& A, const FVector& B, const FVector& P)
{
	const FVector AB = B - A;
	const FVector AP = P - A;
	const float T = FVector::DotProduct(AP, AB) / FVector::DotProduct(AB, AB);
	return T;
}

TTuple<FVector, FVector> FCollisionHelper::GetLocalPoints(FSimplex Simplex, const FVector& Point)
{
	switch(Simplex.Count)
	{
	case 1:
		return {Simplex.A1, Simplex.A2 };
	case 2:
		{
			const float Barycentric  = GetBarycentricScalarLine(Simplex.A, Simplex.B, Point);
			const FVector CartesianA = ConvertBarycentricCoordsToCartesianCoordsLine(Simplex.A1, Simplex.B1, Barycentric);
			const FVector CartesianB = ConvertBarycentricCoordsToCartesianCoordsLine(Simplex.A2, Simplex.B2, Barycentric);
			return { CartesianA, CartesianB };
		}
	case 3:
		{
			const FVector Barycentric = GetBarycentricCoordsTriangle(Simplex.A, Simplex.B, Simplex.C, Point);
			const FVector CartesianA = ConvertBarycentricCoordsToCartesianCoordsTriangle(Simplex.A1, Simplex.B1, Simplex.C1, Barycentric);
			const FVector CartesianB = ConvertBarycentricCoordsToCartesianCoordsTriangle(Simplex.A2, Simplex.B2, Simplex.C2, Barycentric);
			return { CartesianA, CartesianB };
		}
	default:
		return { FVector::ZeroVector, FVector::ZeroVector };
	}
}

FCollisionOutput FCollisionHelper::GJK_Complex(const FCollisionShape& ShapeA, const FTransform& TransformA, const FCollisionShape& ShapeB, const FTransform& TransformB)
{
	FSimplex Simplex;
	FVector Dir = (TransformB.GetLocation() - TransformA.GetLocation()).GetSafeNormal();

	Simplex.A1 = Support(ShapeA, TransformA, -Dir);
	Simplex.A2 = Support(ShapeB, TransformB, Dir);
	Simplex.A = Simplex.A2 - Simplex.A1;
	Simplex.Count = 1;

	FSimplex BestSimplex;
	float BestSupportDifference = -std::numeric_limits<float>::infinity();
	FVector BestPointOnSimplex;

	for(int32 IdxIter = 0; IdxIter < 32; ++IdxIter)
	{
		FVector PointOnSimplex;

		FVector Direction;
			
		switch(Simplex.Count)
		{
		case 1:
			PointOnSimplex = Simplex.A;
			Direction = -Simplex.A;
			break;
		case 2:
			PointOnSimplex = SolveSimplex2(Simplex, Direction);
			break;
		case 3:
			PointOnSimplex = SolveSimplex3(Simplex, Direction);
			break;
		case 4:
			PointOnSimplex = SolveSimplex4(Simplex, Direction);
			break;
		default:
			check(false);
		}

		Direction.Normalize();

		// Early-out to avoid FPU imprecision issues. We're close enough to 0, so it's good enough.
		if(PointOnSimplex.Length() < KINDA_SMALL_NUMBER)
		{
			const TTuple<FVector, FVector> BestPoints = GetLocalPoints(Simplex, PointOnSimplex);
			return FCollisionOutput
			{
				.bDidOverlap = true,
				.Distance = 0.0f,
				.ClosestA = BestPoints.Key,
				.ClosestB = BestPoints.Value,
				.Normal = -Direction.GetSafeNormal()
			};
		}

		FVector SupportA = Support(ShapeA, TransformA, -Direction);
		FVector SupportB = Support(ShapeB, TransformB, Direction);
		FVector Support = SupportB - SupportA;
			
		const float PDotDir = PointOnSimplex.Dot(Direction);
		const float SupportDotDir = Support.Dot(Direction) - KINDA_SMALL_NUMBER;

		// Detect if PointOnSimplex is more extreme than the Support point. In this case, we can't have a better support point.
		if(PDotDir >= SupportDotDir)
		{
			const TTuple<FVector, FVector> BestPoints = GetLocalPoints(Simplex, PointOnSimplex);
			return FCollisionOutput
			{
				.bDidOverlap = false,
				.Distance = static_cast<float>(PointOnSimplex.Length()),
				.ClosestA = BestPoints.Key,
				.ClosestB = BestPoints.Value,
				.Normal = -Direction.GetSafeNormal()
			};
		}

		const float SupportDifference = SupportDotDir - PDotDir;
		if(SupportDifference > BestSupportDifference)
		{
			BestSupportDifference = SupportDifference;
			BestSimplex = Simplex;
			BestPointOnSimplex = PointOnSimplex;
		}

		check(Simplex.Count < 4);

		// Shuffle 'dem points.
		Simplex.ShufflePoints(SupportA, SupportB, Support);

		++Simplex.Count;
	}
		
	const TTuple<FVector, FVector> BestPoints = GetLocalPoints(BestSimplex, BestPointOnSimplex);
	return FCollisionOutput
	{
		.bDidOverlap = false,
		.Distance = static_cast<float>(BestPointOnSimplex.Length()),
		.ClosestA = BestPoints.Key,
		.ClosestB = BestPoints.Value,
		.Normal = -BestPointOnSimplex.GetSafeNormal()
	};
}

FCollisionOutput FCollisionHelper::GJK_Simple(const FCollisionShape& ShapeA, const FTransform& TransformA, const FCollisionShape& ShapeB, const FTransform& TransformB)
{
	FCollisionOutput Output;
	Output.bDidOverlap = false;
		
	TArray<FVector> Simplex;
	FVector Direction = (TransformB.GetLocation() - TransformA.GetLocation()).GetSafeNormal();

	Simplex.Add(Support(ShapeA, TransformA, ShapeB, TransformB, Direction));
	Direction = -Simplex[0];

	int32 CurIters = 0;
	while (++CurIters < 64)
	{
		Output.ClosestA = Support(ShapeA, TransformA, Direction);
		Output.ClosestB = Support(ShapeB, TransformB, -Direction);
			
		Simplex.Add(Output.ClosestA - Output.ClosestB);
		Output.Distance = ApproximateDistanceFromOrigin(Simplex);
		Output.Normal = CalculateNormal(Simplex);
			
		if (Simplex.Last().Dot(Direction) <= 0)
		{
			return Output;
		}

		if (UpdateSimplex(Simplex, Direction) || Output.Distance < KINDA_SMALL_NUMBER)
		{
			Output.Distance = ApproximateDistanceFromOrigin(Simplex);
			Output.Normal = CalculateNormal(Simplex);
			Output.bDidOverlap = true;
			return Output;
		}

	}
		
	return Output;
}

FConservativeAdvancementOutput FCollisionHelper::ConservativeAdvancement(const FCollisionShape& ShapeA, const FTransform& TransformFromA, const FTransform& TransformToA, const FCollisionShape& ShapeB,
	const FTransform& TransformFromB, const FTransform& TransformToB)
{
	FConservativeAdvancementOutput Output;
		
	FCollisionOutput GJKOutput = GJK_Complex(ShapeA, TransformFromA, ShapeB, TransformFromB);
	Output.bInitialOverlap = GJKOutput.bDidOverlap;
		
	if (GJKOutput.bDidOverlap)
	{
		Output.bCollided = true;
		Output.Time = 0.0f;
		return Output;
	}
		
	const FVector LinearVelocityA = TransformToA.GetTranslation() - TransformFromA.GetTranslation();
	const FVector LinearVelocityB = TransformToB.GetTranslation() - TransformFromB.GetTranslation();
	const FVector RelativeLinearVelocity = LinearVelocityA - LinearVelocityB;

	const float AngularVelocityA = TransformToA.GetRotation().AngularDistance(TransformFromA.GetRotation());
	const float AngularVelocityB = TransformToB.GetRotation().AngularDistance(TransformFromB.GetRotation());
		
	const float BoundingRadiusA = GetBoundingRadius(ShapeA, TransformFromA);
	const float BoundingRadiusB = GetBoundingRadius(ShapeB, TransformFromB);
	const float MaxAngularProjectedVelocity = AngularVelocityA * BoundingRadiusA + AngularVelocityB * BoundingRadiusB;
	const float TotalMovement = RelativeLinearVelocity.Length();
		
	if (FMath::IsNearlyZero(TotalMovement))
	{
		Output.Time = 1.0f; // No relative movement; they will never touch during this step.
		return Output;
	}

	float Lambda = 0.0f;
	float NDotVel = RelativeLinearVelocity.Dot(GJKOutput.Normal);
	
	while (GJKOutput.Distance > KINDA_SMALL_NUMBER)
	{
		if(NDotVel + MaxAngularProjectedVelocity <= SMALL_NUMBER)
		{
			Output.Time = 1.0f;
			return Output;
		}

		const float DeltaLambda = GJKOutput.Distance / (NDotVel + MaxAngularProjectedVelocity);
		Lambda += DeltaLambda;
			
		if(Lambda < 0.0f || Lambda > 1.0f || DeltaLambda <= 0.0f)
		{
			Output.Time = 1.0f;
			return Output;
		}
			
		GJKOutput = GJK_Complex(ShapeA, LerpTransform(TransformFromA, TransformToA, Lambda), ShapeB, LerpTransform(TransformFromB, TransformToB, Lambda));
		NDotVel = RelativeLinearVelocity.Dot(GJKOutput.Normal);
		if(GJKOutput.bDidOverlap)
		{
			Output.Time = Lambda;
			Output.bCollided = true;
			Output.ContactNormal = GJKOutput.Normal;
			Output.ContactPoint = GJKOutput.ClosestA;
			return Output;
		}
	}

	Output.Time = 1.0f;
	return Output;
}

void FCollisionHelper::BoxBroadphase(float DeltaTime, const flecs::entity& Entity, const FCollisionShape& CollisionShape, const FPosition& Position, const FVelocity& Velocity,
                                     FCollisionSpatialGrid& CollisionSpatialGrid, TArray<FEntityPositionCache>& CollisionCandidates)
{
	const FCollisionShape& Shape = CollisionShape;
	const FVector MyPosition = Position.Value;
	const FVelocity& TargetVelocity = Velocity;

	// Find the FBox bounds from the target's transform swept along its velocity, accounting for the extents of the collision shape.
	const FVector Extents = FVector(50.0f); //  FVector(FGJKHelper::GetBoundingRadius(Shape, TargetTransform.Value));
	const FVector Sweep = TargetVelocity.Value * DeltaTime;
	const FVector BoundsA = MyPosition - Extents;
	const FVector BoundsB = MyPosition + Sweep + Extents;
	const FBox Bounds = FBox(FVector::Min(BoundsA, BoundsB), FVector::Max(BoundsA, BoundsB));

	CollisionCandidates.Reset();
	CollisionSpatialGrid.GetEntitiesInBox(Entity, Bounds, CollisionCandidates);

	// Sort candidates by square distance
	CollisionCandidates.Sort([MyPosition](const FEntityPositionCache& A, const FEntityPositionCache& B)
	{
		const float DistanceA = (A.Position - MyPosition).SizeSquared();
		const float DistanceB = (B.Position - MyPosition).SizeSquared();

		return DistanceA < DistanceB;
	});
}

bool FCollisionHelper::NarrowPhase(const float DeltaTime, const flecs::entity& Entity, const FCollisionShape& CollisionShape, const FTransformComponent& Transform, const FVector& Velocity,
	const FAngularVelocity& AngularVelocity, const TArray<FEntityPositionCache>& CollisionCandidates, FPosition& Position, TSet<FNarrowPhaseEntityContact>& NarrowPhaseEntityContacts, float& OutTime)
{
	NarrowPhaseEntityContacts.Reset();
	
	const FTransform& TargetTransform = Transform.Value;
	const FVector& TargetAngularVelocity = AngularVelocity.Value;
	const FTransform TargetFinalTransform = FTransform(TargetTransform.GetRotation() * (TargetAngularVelocity * DeltaTime).ToOrientationQuat(), TargetTransform.GetLocation() + Velocity * DeltaTime);

	OutTime = 1.0f;

	// Print num collision candidates
	// GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, FString::Printf(TEXT("Num Collision Candidates: %d"), CollisionCandidates.Num()));
	
	for(const FEntityPositionCache& Candidate : CollisionCandidates)
	{
		FNarrowPhaseEntityContact Contact {
			.EntityHit = Candidate.Entity
		};
		if(Candidate.Entity == Entity || NarrowPhaseEntityContacts.Contains(Contact)) { continue; }
		
		const FTransform& CandidateTransform = Candidate.Entity.get<FTransformComponent>()->Value;
		const FCollisionShape& CandidateShape = *Candidate.Entity.get<FCollisionShape>();
		const FVector CandidateVelocity = Candidate.Entity.has<FVelocity>() ? Candidate.Entity.get<FVelocity>()->Value : FVector::Zero();
		const FVector CandidateAngularVelocity = Candidate.Entity.has<FAngularVelocity>() ? Candidate.Entity.get<FAngularVelocity>()->Value : FVector::Zero();
		const FTransform CandidateFinalTransform = FTransform(CandidateTransform.GetRotation() * (CandidateAngularVelocity * DeltaTime).ToOrientationQuat(), CandidateTransform.GetLocation() + CandidateVelocity * DeltaTime);

		const FConservativeAdvancementOutput SweepOutput = ConservativeAdvancement(CollisionShape, TargetTransform, TargetFinalTransform, CandidateShape, CandidateTransform, CandidateFinalTransform);
		
		if(SweepOutput.bCollided)
		{
			const bool bIsBlockingHit = Entity.has<FOverlapCollision>();
			
			NarrowPhaseEntityContacts.Add({
				.ContactPoint = SweepOutput.ContactPoint,
				.ContactNormal = SweepOutput.ContactNormal,
				.EntityHit = Candidate.Entity,
				.Time = SweepOutput.Time,
				.bIsBlockingHit = bIsBlockingHit
			});

			OutTime = FMath::Min(OutTime, SweepOutput.Time);
			
			// Candidates are pre-sorted by distance, so we can break early if we hit a blocking hit.
			if(bIsBlockingHit) { break; }
		}
	}

	return FMath::IsNearlyEqual(OutTime, 1.0f);
}