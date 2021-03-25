#include<vector>
#include <string>
#include "cuda_runtime.h"
# include <thrust/host_vector.h>
#include"PBD_Basic.cuh"
#include"CCD_Basic.h"
#include  "BufferDebugModule.h"
#include <sstream>

using namespace std;

static const int pbdObjTimer = 0,
pbdSolverTimer = 1,
colliSolverTimer = 2,
shsTimer = 3,
globalTimer = 4;


int gSubStep = 2;
int gPBDIteration = 100; 
int gCollisionPasses = 5;
int gFPS = 24;
float gDeltaTime = 1.0f / gFPS / (float)gSubStep;
int gCollisionResolves = 7;
int gStartFrame = 1;
int gEndFrame = 50;

static const int nModules = 5;

void ContinueSim()
{
	Timer timers[nModules];
	
	HardwareType ht = CPU;
	PBDObject pbdObj;
	pbdObj.ContinueSimInit("D://0324CCDTest//continueSimData//1clothMeshTopol//TestB1LargeClothWithSphereMeshTopol.20.cache",
											"D://0324CCDTest//continueSimData//1ClothConstraint//TestB1LargeClothWithSphereConstraint.20.cache", ht);
	SolverType st = GAUSSSEIDEL;
	pbdObj.SetTimer(&timers[pbdObjTimer]);

	SolverPBD pbdSolver;
	pbdSolver.SetTarget(&pbdObj);
	pbdSolver.SetTimer(&timers[pbdSolverTimer]);

	// three cloth with sphere
	//float3 cellSize = make_float3(2.5f, 2.5f, 2.5f);
	//float3 gridCenter = make_float3(0.0f, 4.0f, 0.0f);
	//float3 gridCenter = make_float3(0.0f, -1.0f, 0.0f);
	//int3 gridSize = make_int3(6, 4, 6);
	// 5 cloth with sphere
	//float3 cellSize = make_float3(0.5f, 0.5f, 0.5f);
	//float3 gridCenter = make_float3(0.0f, 4.0f, 0.0f);
	//int3 gridSize = make_int3(30, 10, 30);
	// 1 large cloth with sphere
	float3 cellSize = make_float3(0.35f, 0.35f, 0.35f);
	float3 gridCenter = make_float3(0.0f, -3.0f, 0.0f);
	uint3 gridSize = make_uint3(32, 23, 32);

	// initialize SH
	SpatialHashSystem shs(pbdObj.constrPBDBuffer.prdPBuffer, pbdObj.meshTopol.indices, CPU);
	shs.SetGridCenter(gridCenter);
	shs.SetGridSize(gridSize);
	shs.SetDivision(cellSize);
	shs.SetTimer(&timers[shsTimer]);
	shs.InitSH();

	CollisionSolver colliSolver;
	colliSolver.SetTarget(&pbdObj);
	colliSolver.SetThickness(0.03f);
	colliSolver.SetIterations(7);
	colliSolver.SetAcceStruct(&shs);
	colliSolver.SetTimer(&timers[colliSolverTimer]);

	// for collision debugging
	colliSolver.m_debugFrameID = 1;
	colliSolver.m_nContact.m_Data.resize(pbdObj.meshTopol.posBuffer.GetSize(), 0);

	int substep = gSubStep;
	int fps = 24;
	float dt = 1.0 / fps / (float)substep;
	int contiCookTimes = 0;
	for (size_t i = gStartFrame; i <= gEndFrame; i++)
	{
		timers[globalTimer].Tick(); 

		BufferVector3f fixedBuffer;
		fixedBuffer.m_Data.resize(pbdObj.meshTopol.posBuffer.GetSize(), make_float3(0.0f, 0.0f, 0.0f));
		for (size_t s = 0; s < gSubStep; s++)
		{
			pbdSolver.Advect(dt);
			pbdSolver.ProjectConstraintWithColli(st, gPBDIteration, &colliSolver, fixedBuffer);			
			colliSolver.afterProjPrdpBuffer = pbdObj.constrPBDBuffer.prdPBuffer;
			printf("------------------------------------frame: %d-------------------------------\n", i);	
			for (int i = 0; i < colliSolver.vfIndices.GetSize(); ++i)
			{
				printf("%d,", colliSolver.vfIndices.m_Data[i]);
			}
			printf("\n");
			pbdSolver.Integration(dt);
			IO::SaveToplogy(pbdObj.meshTopol, "D://0319CCDTest//continueSimData//NewcontiSimData." + to_string((i - 1) * substep + s + 1) + ".cache");
			cout << "topol saved" << endl;
			contiCookTimes++;
		}
		timers[globalTimer].Tock();  // global timer
		PBD_DEBUGTIME(timers[globalTimer].GetFuncTime());
	}
}

void CollisionTest(int colliPasses) // for later data oriented continue sim
{
	float thickness = 0.03f;
	int debugFrameId = 1;
	int colliResolveIterations = 2;
	Topology meshTopol; // 上一帧collision free导出的meshtopol
	BufferVector3f prdPBuffer; // 当前帧导出的posBuffer
	readMeshFromTxt("D://0319CCDTest//singleResolveResult//frame14.txt", meshTopol);
	readBufferFromTxt("D://0319CCDTest//singleResolveResult//frame17.txt", prdPBuffer);
	
	float3 cellSize = make_float3(0.35f, 0.35f, 0.35f);
	float3 gridCenter = make_float3(0.0f, -3.0f, 0.0f);
	uint3 gridSize = make_uint3(32, 23, 32);
	SpatialHashSystem shs(prdPBuffer, meshTopol.indices, CPU, gridCenter, gridSize, cellSize);
	shs.InitSH();

	BufferInt vfIndices;
	BufferInt resolveTimes;
	for (int cp = 0; cp < colliPasses; ++cp)
	{
		ContactData contactData;
		CCD_SH(contactData, shs, meshTopol, prdPBuffer, thickness);
		CollisionResolve(meshTopol, prdPBuffer, contactData, colliResolveIterations, thickness, debugFrameId, vfIndices, resolveTimes);
		printf("--------%d collision resolve----------\n", cp);
	}
	for (int i = 0; i < vfIndices.GetSize(); ++i)
	{
		printf("%d,", vfIndices.m_Data[i]);
	}
	printf("\n");
	Topology afterResolveTopol;
	prdPBuffer.SetName("P");
	afterResolveTopol.indices = meshTopol.indices;
	afterResolveTopol.primList = meshTopol.primList;
	afterResolveTopol.posBuffer = prdPBuffer;
	afterResolveTopol.indices.SetName("indices");
	afterResolveTopol.primList.SetName("primList");
	afterResolveTopol.posBuffer.SetName("P");
	IO::SaveToplogy(afterResolveTopol, "D://0319CCDTest//singleResolveResult//singleResolveResult." + to_string(17) + ".cache");
	printf("---------------------------resolved topol saved--------------------\n");
	
}

static bool gEnableSelf = true;
void RegularSim();
void RegularSimColliWithProj();

int main()
{
	BufferDebugModule::GetInstance()->Load();
	//RegularSim();
	//RegularSimColliWithProj();
	ContinueSim();
	//CollisionTest(gCollisionPasses);
	return 0;
}

void RegularSim()
{
	Timer timers[nModules];
	float dampingRate = 0.9f;
	float3 gravity = make_float3(0.0, -10.0, 0.0);
	float stiffnessSetting[1] = { 1.0f };
	HardwareType ht = CPU;
	SolverType st = GAUSSSEIDEL;

	//string topolFileName = "D://0310ContinuousCollisionDectection//ccdTestData//InitTopolLowHard.txt";
	//string distConstrFileName = "D://0310ContinuousCollisionDectection//ccdTestData//DistanceConstrLowHard.txt";
	//string topolFileName = "D://0310ContinuousCollisionDectection//ccdTestData//5SheetsInitTopol.txt";
	//string distConstrFileName = "D://0310ContinuousCollisionDectection//ccdTestData//5SheetsDistanceConstr.txt";
	string topolFileName = "D://0319CCDTest//1ClothWithSphereTopol.txt";
	string distConstrFileName = "D://0319CCDTest//1ClothWithSphereConstr.txt";

	PBDObject pbdObj(dampingRate, gravity, ht);
	pbdObj.SetConstrOption(DISTANCE, stiffnessSetting);
	pbdObj.SetTimer(&timers[pbdObjTimer]);
	pbdObj.Init(topolFileName, distConstrFileName);

	SolverPBD pbdSolver;
	pbdSolver.SetTarget(&pbdObj);
	pbdSolver.SetTimer(&timers[pbdSolverTimer]);

	// three cloth with sphere
	//float3 cellSize = make_float3(2.5f, 2.5f, 2.5f);
	//float3 gridCenter = make_float3(0.0f, 4.0f, 0.0f);
	//float3 gridCenter = make_float3(0.0f, -1.0f, 0.0f);
	//int3 gridSize = make_int3(6, 4, 6);
	// 5 cloth with sphere
	//float3 cellSize = make_float3(0.5f, 0.5f, 0.5f);
	//float3 gridCenter = make_float3(0.0f, 4.0f, 0.0f);
	//int3 gridSize = make_int3(30, 10, 30);
	// 1 large cloth with sphere
	float3 cellSize = make_float3(0.35f, 0.35f, 0.35f);
	float3 gridCenter = make_float3(0.0f, -3.0f, 0.0f);
	uint3 gridSize = make_uint3(32, 23, 32);

	// initialize SH
	SpatialHashSystem shs(pbdObj.constrPBDBuffer.prdPBuffer, pbdObj.meshTopol.indices, CPU);
	shs.SetGridCenter(gridCenter);
	shs.SetGridSize(gridSize);
	shs.SetDivision(cellSize);
	shs.SetTimer(&timers[shsTimer]);
	shs.InitSH();

	CollisionSolver colliSolver;
	colliSolver.SetTarget(&pbdObj);
	colliSolver.SetThickness(0.03f);
	colliSolver.SetIterations(gCollisionResolves);
	colliSolver.SetAcceStruct(&shs);
	colliSolver.SetTimer(&timers[colliSolverTimer]);

	// for collision debugging
	colliSolver.m_nContact.m_Data.resize(pbdObj.meshTopol.posBuffer.GetSize(), 0);

	string meshPath = "D://0319CCDTest//continueSimData//meshTopol//5NewLargeClothMeshTopol.";
	string constrPath = "D://0319CCDTest//continueSimData//constraint//5NewLargeClothConstraint.";
	string collisionPath = "D://0319CCDTest//continueSimData//collision//5NewLargeClothCollision.";

	int cookTimes = 0;
	int contiCookTimes = 0;
	for (size_t i = gStartFrame; i <= gEndFrame; i++)
	{
		//if (i >= 20)
		//{
		   // BufferDebugModule::GetInstance()->PushStack(contiCookTimes, "ENTRY_PRDB_检查输入", pbdObj.constrPBDBuffer.prdPBuffer);
		//}
		timers[globalTimer].Tick();  // global timer
		for (size_t s = 0; s < gSubStep; s++)
		{
			pbdSolver.Advect(gDeltaTime);
			pbdSolver.ProjectConstraint(st, gPBDIteration);
			//if (i >= 20)
			//{
			   // BufferDebugModule::GetInstance()->PushStack( contiCookTimes, "算完_ProjectConstraint", pbdObj.constrPBDBuffer.prdPBuffer);
			//}
			//POST Rendering
			if (gEnableSelf)
			{
				for (int col = 0; col < gCollisionPasses; col++)
				{
					colliSolver.CCD_SH();
					colliSolver.CollisionResolve();
				}
			}

			pbdSolver.Integration(gDeltaTime);
			string path = to_string((i - 1) * gSubStep + s + 1) + ".cache";
			pbdObj.SaveMeshTopol(meshPath + path);
			pbdObj.SaveConstraint(constrPath + path);
			colliSolver.SaveCollision(collisionPath + path);
			//IO::SaveToplogy(pbdObj.meshTopol, "D://0310ContinuousCollisionDectection//ContinuousSimData//5ccdTestLow." + to_string((i-1)*10 + s +1)  + ".cache");
			//cout << "topol saved" << endl;

			cookTimes++;
		}
		IO::SaveToplogy(pbdObj.meshTopol, "D://0319CCDTest//1largeClothOutput//5NewLargeClothWithSphere." + to_string(i) + ".cache");
		printf("---------------------------frame %d topol saved--------------------\n", i);
		timers[globalTimer].Tock();  // global timer
		PBD_DEBUGTIME(timers[globalTimer].GetFuncTime());
	}
}

void RegularSimColliWithProj()
{
	Timer timers[nModules];
	float dampingRate = 0.9f;
	float3 gravity = make_float3(0.0, -10.0, 0.0);
	float stiffnessSetting[1] = { 1.0f };
	HardwareType ht = CPU;
	SolverType st = GAUSSSEIDEL;

	//string topolFileName = "D://0324CCDTest//6SheetsInitTopol.txt";
	//string distConstrFileName = "D://0324CCDTest//6SheetsDistanceConstr.txt";
	string topolFileName = "D://0319CCDTest//1ClothWithSphereTopol.txt";
	string distConstrFileName = "D://0319CCDTest//1ClothWithSphereConstr.txt";

	PBDObject pbdObj(dampingRate, gravity, ht);
	pbdObj.SetConstrOption(DISTANCE, stiffnessSetting);
	pbdObj.SetTimer(&timers[pbdObjTimer]);
	pbdObj.Init(topolFileName, distConstrFileName);

	SolverPBD pbdSolver;
	pbdSolver.SetTarget(&pbdObj);
	pbdSolver.SetTimer(&timers[pbdSolverTimer]);

	// 6 large cloth with sphere
	//float3 cellSize = make_float3(1.0f, 1.0f, 1.0f);
	//float3 gridCenter = make_float3(0.0f, -4.0f, 0.0f);
	//uint3 gridSize = make_uint3(25, 15, 25);
	// 1 large cloth with sphere
	float3 cellSize = make_float3(0.35f, 0.35f, 0.35f);
	float3 gridCenter = make_float3(0.0f, -3.0f, 0.0f);
	uint3 gridSize = make_uint3(32, 23, 32);

	// initialize SH
	SpatialHashSystem shs(pbdObj.constrPBDBuffer.prdPBuffer, pbdObj.meshTopol.indices, CPU);
	shs.SetGridCenter(gridCenter);
	shs.SetGridSize(gridSize);
	shs.SetDivision(cellSize);
	shs.SetTimer(&timers[shsTimer]);
	shs.InitSH();

	CollisionSolver colliSolver;
	colliSolver.SetTarget(&pbdObj);
	colliSolver.SetThickness(0.03f);
	colliSolver.SetIterations(gCollisionResolves);
	colliSolver.SetAcceStruct(&shs);
	colliSolver.SetTimer(&timers[colliSolverTimer]);

	// for collision debugging
	colliSolver.m_nContact.m_Data.resize(pbdObj.meshTopol.posBuffer.GetSize(), 0);

	//string meshPath = "D://0324CCDTest//continueSimData//meshTopol//TestB6LargeClothWithSphereMeshTopol.";
	//string constrPath = "D://0324CCDTest//continueSimData//constraint//TestB6LargeClothWithSphereConstraint.";
	//string collisionPath = "D://0324CCDTest//continueSimData//collision//TestB6LargeClothWithSphereCollision.";
	string meshPath = "D://0324CCDTest//continueSimData//1clothMeshTopol//TestB1LargeClothWithSphereMeshTopol.";
	string constrPath = "D://0324CCDTest//continueSimData//1ClothConstraint//TestB1LargeClothWithSphereConstraint.";
	string collisionPath = "D://0324CCDTest//continueSimData//1clothCollision//TestB1LargeClothWithSphereCollision.";

	int contiCookTimes = 0;
	for (size_t i = gStartFrame; i <= gEndFrame; i++)
	{
		timers[globalTimer].Tick();  // global timer
		BufferVector3f fixedBuffer;
		fixedBuffer.m_Data.resize(pbdObj.meshTopol.posBuffer.GetSize(), make_float3(0.0f, 0.0f, 0.0f));
		for (size_t s = 0; s < gSubStep; s++)
		{
			pbdSolver.Advect(gDeltaTime);
			if (CONTACT_MERGE)
			{
				colliSolver.contactData.ctxs.m_Data.clear();
				colliSolver.contactData.ctxStartNum.m_Data.clear();
				colliSolver.contactData.ctxIndices.m_Data.clear();
			}
			pbdSolver.ProjectConstraintWithColli(st, gPBDIteration, &colliSolver, fixedBuffer);
			pbdSolver.Integration(gDeltaTime);
			string path = to_string((i - 1) * gSubStep + s + 1) + ".cache";
			pbdObj.SaveMeshTopol(meshPath + path);
			pbdObj.SaveConstraint(constrPath + path);
			//colliSolver.SaveCollision(collisionPath + path);
			//IO::SaveToplogy(pbdObj.meshTopol, "D://0310ContinuousCollisionDectection//ContinuousSimData//5ccdTestLow." + to_string((i-1)*10 + s +1)  + ".cache");
			//cout << "topol saved" << endl;
		}
		fixedBuffer.SetName("P");
		//IO::SaveToplogy(pbdObj.meshTopol, "D://0324CCDTest//6largeClothOutput//TestB6LargeClothWithSphere." + to_string(i) + ".cache");
		//printf("---------------------------frame %d topol saved--------------------\n", i);
		//IO::SaveBuffer(fixedBuffer, "D://0324CCDTest//6largeClothOutput//TestB6LargeClothWithSphereFixedBuffer." + to_string(i) + ".cache");
		//printf("---------------------------frame %d fixedBuffer saved--------------------\n", i);
		IO::SaveToplogy(pbdObj.meshTopol, "D://0324CCDTest//1largeClothOutput//TestB1LargeClothWithSphere." + to_string(i) + ".cache");
		printf("---------------------------frame %d topol saved--------------------\n", i);
		IO::SaveBuffer(fixedBuffer, "D://0324CCDTest//1largeClothOutput//TestB1LargeClothWithSphereFixedBuffer." + to_string(i) + ".cache");
		printf("---------------------------frame %d fixedBuffer saved--------------------\n", i);
		timers[globalTimer].Tock();  // global timer
		PBD_DEBUGTIME(timers[globalTimer].GetFuncTime());
	}
}

