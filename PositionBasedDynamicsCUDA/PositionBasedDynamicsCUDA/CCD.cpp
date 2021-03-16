#include "CCD_Basic.h"

float DistanceCCD(float3 p1, float3 p2)
{ 
	return powf(powf((p1.x - p2.x), 2) + powf((p1.y - p2.y), 2) + powf((p1.z - p2.z), 2), 0.5); 
}

float3 pos(const float3 pos, const float3 prdPos, double t)
{
	return pos + t * (prdPos - pos);
}

bool IsSameSide(float3 vtx, float3 pvtx, float3 n)
{
	float3 v = vtx - pvtx;
	float d = dot(v, n);
	if (d >= 0)
	{
		return true;
	}
	else if (d < 0)
	{
		return false;
	}
}

float Point2Plane(float3 vtx, float3 p1, float3 p2, float3 p3)
{
	//return dot(normalize(cross(p2 - p1, p3 - p2)), vtx - p1);
	float d;
	float3 AB = p2 - p1;
	float3 AC = p3 - p1;
	float3 N = cross(AB, AC);
	float Ax = N.x;
	float By = N.y;
	float Cz = N.z;
	float D = -(Ax * p1.x + By * p1.y + Cz * p1.z);
	float mod = Ax * vtx.x + By * vtx.y + Cz * vtx.z + D;
	float area = sqrt(Ax * Ax + By * By + Cz * Cz);
	d = abs(mod) / area;
	return d;
}

// Solving cubic equations
// solves a x^3 + b x^2 + c x + d == 0
int solve_cubic(double a, double b, double c, double d, double x[3]) {
	double xc[2];
	int ncrit = solve_quadratic(3 * a, 2 * b, c, xc);
	if (ncrit == 0) {
		x[0] = newtons_method(a, b, c, d, xc[0], 0);
		return 1;
	}
	else if (ncrit == 1) {// cubic is actually quadratic
		return solve_quadratic(b, c, d, x);
	}
	else {
		double yc[2] = { d + xc[0] * (c + xc[0] * (b + xc[0] * a)),
						d + xc[1] * (c + xc[1] * (b + xc[1] * a)) };
		int i = 0;
		if (yc[0] * a >= 0)
			x[i++] = newtons_method(a, b, c, d, xc[0], -1);
		if (yc[0] * yc[1] <= 0) {
			int closer = abs(yc[0]) < abs(yc[1]) ? 0 : 1;
			x[i++] = newtons_method(a, b, c, d, xc[closer], closer == 0 ? 1 : -1);
		}
		if (yc[1] * a <= 0)
			x[i++] = newtons_method(a, b, c, d, xc[1], 1);
		return i;
	}
}

double newtons_method(double a, double b, double c, double d, double x0, int init_dir)
{
	if (init_dir != 0) {
		// quadratic approximation around x0, assuming y' = 0
		double y0 = d + x0 * (c + x0 * (b + x0 * a)),
			ddy0 = 2 * b + x0 * (6 * a);
		x0 += init_dir * sqrt(abs(2 * y0 / ddy0));
	}
	for (int iter = 0; iter < 100; iter++) {
		double y = d + x0 * (c + x0 * (b + x0 * a));
		double dy = c + x0 * (2 * b + x0 * 3 * a);
		if (dy == 0)
			return x0;
		double x1 = x0 - y / dy;
		if (abs(x0 - x1) < 1e-6)
			return x0;
		x0 = x1;
	}
	return x0;
}

int solve_quadratic(double a, double b, double c, double x[2])
{
	// http://en.wikipedia.org/wiki/Quadratic_formula#Floating_point_implementation
	double d = b * b - 4 * a * c;
	if (d < 0) {
		x[0] = -b / (2 * a);
		return 0;
	}
	double q = -(b + sgn(b) * sqrt(d)) / 2;
	int i = 0;
	if (abs(a) > 1e-12 * abs(q))
		x[i++] = q / a;
	if (abs(q) > 1e-12 * abs(c))
		x[i++] = c / q;
	if (i == 2 && x[0] > x[1])
		swap(x[0], x[1]);
	return i;
}

float3 BarycentricCoord(float3 pos, float3 p0, float3 p1, float3 p2)
{
	float3 AB = p0 - p2;
	float3 AC = p1 - p2;
	float3 AP = pos - p2;
	float dot00 = dot(AC, AC);
	float dot01 = dot(AC, AB);
	float dot02 = dot(AC, AP);
	float dot11 = dot(AB, AB);
	float dot12 = dot(AB, AP);
	float inverDeno = 1 / (dot00 * dot11 - dot01 * dot01);
	float v = (dot11 * dot02 - dot01 * dot12) * inverDeno;
	float u = (dot00 * dot12 - dot01 * dot02) * inverDeno;
	float w = 1 - u - v;
	return make_float3(u, v, w);
}
// -------------------------- Collision Solver--------------------------------------

bool CollisionSolver::VFTest(float3 vtx_o, float3 p1_o, float3 p2_o, float3 p3_o,
			float3 vtx_p, float3 p1_p, float3 p2_p, float3 p3_p,  
	        Contact& contact, int i0, int i1, int i2, int i3)
{		
	if (CCDCollisionTest(vtx_o, p1_o, p2_o, p3_o, vtx_p, p1_p, p2_p, p3_p, Contact::VF, contact, i0, i1, i2, i3))
	{
		//printf("vf contact\n");
		contact.type = Contact::VF;
		return true;
	}
	else if(VFTestDCD(vtx_o, p1_o, p2_o, p3_o, vtx_p, p1_p, p2_p, p3_p, contact))
	{
		//printf("vf thickness contact\n");
		contact.type = Contact::VFDCD;
		return true;
	}
	else
		return false;
}

bool CollisionSolver::VFTestDCD(float3 vtx_o, float3 p1_o, float3 p2_o, float3 p3_o,
	float3 vtx_p, float3 p1_p, float3 p2_p, float3 p3_p,
	Contact& contact)
{
	float d = Point2Plane(vtx_p, p1_p, p2_p, p3_p);		
	if (d > 2 * m_thickness)
		return false;
	contact.w[0] = 1;
	contact.w[1] = BarycentricCoord(vtx_p, p1_p, p2_p, p3_p).x;
	contact.w[2] = BarycentricCoord(vtx_p, p1_p, p2_p, p3_p).y;
	contact.w[3] = BarycentricCoord(vtx_p, p1_p, p2_p, p3_p).z; 
	contact.n = normalize(cross(normalize(p2_p - p1_p), normalize(p3_p - p1_p)));
	bool inside;
	inside = (min(contact.w[1], contact.w[2], contact.w[3]) >= 1e-6);
	if (!inside)
		return false;
	return true;
}

bool CollisionSolver::CCDCollisionTest(float3 vtx_o, float3 p1_o, float3 p2_o, float3 p3_o,
	                         float3 vtx_p, float3 p1_p, float3 p2_p, float3 p3_p, 
	                         Contact::Type type, Contact& contact, int i0, int i1, int i2, int i3)
{
	const float3& x0 = vtx_o, v0 = vtx_p - x0;  // x0: V's pos   v0: V's prdPos - pos
	float3 x1 = p1_o - x0, x2 = p2_o - x0, x3 = p3_o - x0; // x: p's pos - V's pos
	float3 v1 = (p1_p - p1_o) - v0, v2 = (p2_p - p2_o) - v0, v3 = (p3_p - p3_o) - v0; // v: (p's prdPos - p's pos) - v0
	double a0 = stp(x1, x2, x3), a1 = stp(v1, x2, x3) + stp(x1, v2, x3) + stp(x1, x2, v3), a2 = stp(x1, v2, v3) + stp(v1, x2, v3) + stp(v1, v2, x3), a3 = stp(v1, v2, v3);
	double t[4];
	int nsol = solve_cubic(a3, a2, a1, a0, t); // number of solution
	t[nsol] = 1; // also check at end of timestep
	for (int i = 0; i < nsol; i++) 
	{
		if (t[i] < 0 || t[i] > 1) 
			continue;
		contact.t = t[i];
		float3 x0 = pos(vtx_o, vtx_p, t[i]), x1 = pos(p1_o, p1_p, t[i]), x2 = pos(p2_o, p2_p, t[i]), x3 = pos(p3_o, p3_p, t[i]);
		float3& n = contact.n;
		double* w = contact.w;
		double d; // is vtx on tirangle
		bool inside;
		contact.n = normalize(cross(p2_p - p1_p, p3_p - p1_p));
		 //compute weight and normal
		if (type == Contact::VF) 
		{
			float3 cn = normalize(cross(x2 - x1, x3 - x1));
			d = dot(x0 - x1, cn);
			float3 weight = BarycentricCoord(x0, x1, x2, x3);
			contact.w[0] = 1;
			contact.w[1] = weight.x;
			contact.w[2] = weight.y;
			contact.w[3] = weight.z;
			inside = (min(w[1], w[2], w[3]) >= 1e-6);
		/*	if (i0 == 173 )
			{
				printf("-------(%d, %d, %d)-------\n", i1, i2, i3);
				printf("x0: (%f, %f, %f)\n", x0.x, x0.y, x0.z);
				printf("x1: (%f, %f, %f)\n", x1.x, x1.y, x1.z);
				printf("x2: (%f, %f, %f)\n", x2.x, x2.y, x2.z);
				printf("x3: (%f, %f, %f)\n", x3.x, x3.y, x3.z);				
				printf("w: (%f, %f, %f)\n", w[1], w[2], w[3]);
				printf("normal: (%f, %f, %f)\n", n.x, n.y, n.z);
				printf("normal: (%f, %f, %f)\n", contact.n.x, contact.n.y, contact.n.z);
				cout << "inside: " << inside << endl;
				printf("contact t: %f\n", contact.t);
				printf("d :%f\n", abs(d));
			}*/
		}
		else {// Impact::EE
			d = signed_ee_distance(x0, x1, x2, x3, &n, w);
			inside = (min(w[0], w[1], -w[2], -w[3]) >= -1e-6);
		}
		if (abs(d) < 1e-6 && inside)
		{	
			return true;
		}			
	}
	return false;
}

void CollisionSolver::VFResolve(float3 vtxPos, float3 p1Pos, float3 p2Pos, float3 p3Pos,
	                      float3& vtxPrd, float3& p1Prd, float3& p2Prd, float3& p3Prd, 
	                      Contact contact, int i0, int i1, int i2, int i3)
{
	//cout << __FUNCTION__ << endl;
	float3 fn = normalize(cross(p2Pos - p1Pos, p3Pos - p1Pos));
	bool sameSide = IsSameSide(vtxPos, p1Pos, fn);
	//printf("normal: (%f, %f, %f)\n", contact.n.x, contact.n.y, contact.n.z);
	//cout << "sameSide:" << sameSide << endl;
	//printf("------344 vtxPrd: (%f, %f, %f)--------\n", m_prdPBuffer.m_Data[344].x, m_prdPBuffer.m_Data[344].y, m_prdPBuffer.m_Data[344].z);
	//printf("vtxPos: (%f, %f, %f)\n", vtxPos.x, vtxPos.y, vtxPos.z);
	//printf("p1Pos: (%f, %f, %f)\n", p1Pos.x, p1Pos.y, p1Pos.z);
	//printf("p2Pos: (%f, %f, %f)\n", p2Pos.x, p2Pos.y, p2Pos.z);
	//printf("p3Pos: (%f, %f, %f))\n", p3Pos.x, p3Pos.y, p3Pos.z);
	//printf("vtxPrd: (%f, %f, %f)\n", vtxPrd.x, vtxPrd.y, vtxPrd.z);
	//printf("p1Prd: (%f, %f, %f)\n", p1Prd.x, p1Prd.y, p1Prd.z);
	//printf("p2Prd: (%f, %f, %f)\n", p2Prd.x, p2Prd.y, p2Prd.z);
	//printf("p3Prd: (%f, %f, %f)\n", p3Prd.x, p3Prd.y, p3Prd.z);
	float3 n = contact.n;
	float depth = Point2Plane(vtxPrd, p1Prd, p2Prd, p3Prd);
	float dp;
	float sw = contact.w[1] * contact.w[1] + contact.w[2] * contact.w[2] + contact.w[3] * contact.w[3];
	if (sameSide)
	{
		if (contact.type == Contact::VF)
		{
			dp = depth + 2 * m_thickness;
		}
		else if(contact.type == Contact::VFDCD)
		{
			dp = 2 * m_thickness - depth;
		}
		vtxPrd += n * dp;
		p1Prd += -n * dp * contact.w[1] / sw;
		p2Prd += -n * dp * contact.w[2] / sw;
		p3Prd += -n * dp * contact.w[3] / sw;

		//printf("normal: (%f, %f, %f) \n", n.x, n.y, n.z);
		//printf("dp: %f\n", dp);
		//printf("w1: (%f) ", (contact.w[1] / sw));
		//printf("w2: (%f) ", (contact.w[2] / sw));
		//printf("w3: (%f)\n", (contact.w[3] / sw));
		//printf("change1: (%f) ", (dp * contact.w[1] / sw));
		//printf("change2: (%f) ", (dp * contact.w[2] / sw));
		//printf("change3: (%f)\n", (dp * contact.w[3] / sw));	
		//p1Prd += -n * dp * contact.w[1];
		//p2Prd += -n * dp * contact.w[2];
		//p3Prd += -n * dp * contact.w[3];

	}
	else
	{
		if (contact.type == Contact::VF)
		{
			printf("vfccd");
			dp = depth + 2 * m_thickness;
		}
		else if (contact.type == Contact::VFDCD)
		{
			printf("thickness\n");
			dp = 2 * m_thickness - depth;
		}
		vtxPrd += -n * dp;
		p1Prd += n * dp * contact.w[1] / sw;
		p2Prd += n * dp * contact.w[2] / sw;
		p3Prd += n * dp * contact.w[3] / sw;
	}
	//printf("normal: (%f, %f, %f) \n", n.x, n.y, n.z); 
	//printf("point id: %d ", i0);
	//printf("vtxPrd: (%f, %f, %f)\n", vtxPrd.x, vtxPrd.y, vtxPrd.z);
	//printf("point id: %d ", i1);
	//printf("p1Prd: (%f, %f, %f)\n", p1Prd.x, p1Prd.y, p1Prd.z);
	//printf("point id: %d ", i2);
	//printf("p2Prd: (%f, %f, %f)\n", p2Prd.x, p2Prd.y, p2Prd.z);
	//printf("point id: %d ", i3);
	//printf("p3Prd: (%f, %f, %f)\n", p3Prd.x, p3Prd.y, p3Prd.z);
	//printf("depth: %f\n", depth);
	//printf("dp: %f\n", dp);
	//printf("------344 vtxPrd: (%f, %f, %f)--------\n", m_prdPBuffer.m_Data[344].x, m_prdPBuffer.m_Data[344].y, m_prdPBuffer.m_Data[344].z);
	/*printf("vtxPrd: (%f, %f, %f)\n", vtxPrd.x, vtxPrd.y, vtxPrd.z);
	printf("p1Prd: (%f, %f, %f)\n", p1Prd.x, p1Prd.y, p1Prd.z);
	printf("p2Prd: (%f, %f, %f)\n", p2Prd.x, p2Prd.y, p2Prd.z);
	printf("p3Prd: (%f, %f, %f)\n", p3Prd.x, p3Prd.y, p3Prd.z);*/
}

void CollisionSolver::CollisionResolve(ContactData& ctxData)
{
	// for ctx : ctxData.ctxs
		// if(Contact::VF)
				// VFResolve(float3 vtxPos, float3 p1Pos, float3 p2Pos, float3 p3Pos,
									// float3 vtxPrd, float3 p1Prd, float3 p2Prd, float3 p3Prd,
		                            // Contact ctx)
		// if(Contact::EE)
				// EEResolve()
}

void CollisionSolver::CollisionResolve()
{
	//cout << __FUNCTION__ << endl;
	//for (int i = 0; i < m_iterations; ++i)
	//{
		//for (int i = 0; i < m_Topol->posBuffer.GetSize(); ++i)
			//{
			//	printf(" (%f, %f, %f)", m_Topol->posBuffer.m_Data[i].x, m_Topol->posBuffer.m_Data[i].y, m_Topol->posBuffer.m_Data[i].z);
			//}
		Contact contact;
		int start, i0, i1, i2, i3;
		float3 vtxPos, p1Pos, p2Pos, p3Pos;
		auto ctxIndices = &(contactData.ctxIndices);
		auto ctxStartNum = &(contactData.ctxStartNum);
		auto ctxList = &(contactData.ctxs);
		auto posBuffer = &(m_pbdObj->meshTopol.posBuffer);
		auto m_prdPBuffer = &(m_pbdObj->constrPBDBuffer.prdPBuffer);
		for (int ctxId = 0; ctxId < ctxList->GetSize(); ++ctxId)
		{
			contact = ctxList->m_Data[ctxId];
			start = ctxStartNum->m_Data[ctxId].x;
			i0 = ctxIndices->m_Data[start];
			i1 = ctxIndices->m_Data[start + 1];
			i2 = ctxIndices->m_Data[start + 2];
			i3 = ctxIndices->m_Data[start + 3];
			vtxPos = posBuffer->m_Data[i0];
			p1Pos = posBuffer->m_Data[i1];
			p2Pos = posBuffer->m_Data[i2];
			p3Pos = posBuffer->m_Data[i3];
			if (Contact::VF || Contact::VFDCD)
			{
				// 如果当前位置关系和collision-free的位置关系相同，且距离大于二倍thickness， 就不修了
				float3 n = normalize(cross(p2Pos - p1Pos, p3Pos - p1Pos));
				bool fRelatedPos = IsSameSide(vtxPos, p1Pos, n);
				bool cRelatedPos = IsSameSide(m_prdPBuffer->m_Data[i0], m_prdPBuffer->m_Data[i1], contact.n);
				//if (i0 == 344 || i1 == 344 || i2 == 344 || i3 == 344)
				//{
				//	cout << "fRelatedPos:" << fRelatedPos << endl;
				//	cout << "cRelatedPos:" << cRelatedPos << endl;
				//	if (cRelatedPos == fRelatedPos &&
				//		(Point2Plane(m_prdPBuffer.m_Data[i0], m_prdPBuffer.m_Data[i1], m_prdPBuffer.m_Data[i2], m_prdPBuffer.m_Data[i3]) > 2 * m_Thickness))
				//		continue;
				//	else
				//	{
				//		VFResolve(vtxPos, p1Pos, p2Pos, p3Pos,
				//			m_prdPBuffer.m_Data[i0], m_prdPBuffer.m_Data[i1], m_prdPBuffer.m_Data[i2], m_prdPBuffer.m_Data[i3],
				//			contact, i0, i1, i2, i3);
				//	}
				//}
				if (cRelatedPos == fRelatedPos &&
					(Point2Plane(m_prdPBuffer->m_Data[i0], m_prdPBuffer->m_Data[i1], m_prdPBuffer->m_Data[i2], m_prdPBuffer->m_Data[i3]) > 2 * m_thickness))
					continue;
				else
				{
					VFResolve(vtxPos, p1Pos, p2Pos, p3Pos,
						m_prdPBuffer->m_Data[i0], m_prdPBuffer->m_Data[i1], m_prdPBuffer->m_Data[i2], m_prdPBuffer->m_Data[i3],
						contact, i0, i1, i2, i3);
				}

				//printf("vtxPrd: (%f, %f, %f)\n", vtxPrd.x, vtxPrd.y, vtxPrd.z);
				//printf("p1Prd: (%f, %f, %f)\n", p1Prd.x, p1Prd.y, p1Prd.z);
				//printf("p2Prd: (%f, %f, %f)\n", p2Prd.x, p2Prd.y, p2Prd.z);
				//printf("p3Prd: (%f, %f, %f)\n", p3Prd.x, p3Prd.y, p3Prd.z);
			}
			//if (Contact::EE)
			//{
			//	EEResolve()
			//}
		}
	//}
}

//接口待确定
void CollisionSolver::CCD_N2()
{
	cout << __FUNCTION__ << endl;
	contactData.ctxs.m_Data.clear();
	contactData.ctxStartNum.m_Data.clear();
	contactData.ctxIndices.m_Data.clear();
	auto indices = &(m_pbdObj->meshTopol.indices);
	auto posBuffer = &(m_pbdObj->meshTopol.posBuffer);
	auto triList = &(m_pbdObj->meshTopol.primList);
	printf("primList:(%d) \n", triList->GetSize());
	//printf("primList:(%d, %d) \n", triList->m_Data[0].x, triList->m_Data[0].y);
	auto m_prdPBuffer = &(m_pbdObj->constrPBDBuffer.prdPBuffer);
	float3 vtxPos, triPos1, triPos2, triPos3, vtxPrdP, triPrdP1, triPrdP2, triPrdP3;
	int i0, i1, i2, i3;
	for (int triId = 0; triId < triList->GetSize(); ++triId)
	{
		int start = triList->m_Data[triId].x;
		int num = triList->m_Data[triId].y;
		for (int vtxId = start; vtxId < start + num; ++vtxId)
		{
			i0 = indices->m_Data[vtxId];
			vtxPos = posBuffer->m_Data[i0];
			vtxPrdP = m_prdPBuffer->m_Data[i0];
			for (int nbtriId = 0; nbtriId < triList->GetSize(); ++nbtriId)
			{
				int start = triList->m_Data[nbtriId].x;
				i1 = indices->m_Data[start];
				i2 = indices->m_Data[start + 1];
				i3 = indices->m_Data[start + 2];
				triPos1 = posBuffer->m_Data[i1];
				triPos2 = posBuffer->m_Data[i2];
				triPos3 = posBuffer->m_Data[i3];
				triPrdP1 = m_prdPBuffer->m_Data[i1];
				triPrdP2 = m_prdPBuffer->m_Data[i2];
				triPrdP3 = m_prdPBuffer->m_Data[i3];
				if (nbtriId == triId)
					continue;
				Contact contact;
				if (VFTest(vtxPos, triPos1, triPos2, triPos3, vtxPrdP, triPrdP1, triPrdP2, triPrdP3, contact, i0, i1, i2, i3))
				{
					contactData.ctxs.m_Data.push_back(contact);
					contactData.ctxStartNum.m_Data.push_back(make_int2(contactData.ctxIndices.GetSize(), 4));
					contactData.ctxIndices.m_Data.push_back(i0);
					contactData.ctxIndices.m_Data.push_back(i1);
					contactData.ctxIndices.m_Data.push_back(i2);
					contactData.ctxIndices.m_Data.push_back(i3);
				}
			}
		}
	}
	// for t : triangles
		// for vtx : t.vertexs
			// for nbt : nbTriangle //记得去掉与当前三角形判断
				/*if( DoVF(vtx,float3 p0,float3 p1,float3 p2))
					ctxData.xxxx.push*/
		// for each edge
			// for each Triangle
				// for each edge
				// DoEE()
}

void CollisionSolver::CCD_SH()
{
	contactData.ctxs.m_Data.clear();
	contactData.ctxStartNum.m_Data.clear();
	contactData.ctxIndices.m_Data.clear();
	//cout << __FUNCTION__ << endl;
	auto indices = &(m_pbdObj->meshTopol.indices);
	auto posBuffer = &(m_pbdObj->meshTopol.posBuffer);
	auto triList = &(m_pbdObj->meshTopol.primList);
	auto prdPBuffer = &(m_pbdObj->constrPBDBuffer.prdPBuffer);
	BufferInt neighborList;   // reserved for SH Find neighbor results
	float3 vtxPos, triPos1, triPos2, triPos3, vtxPrdP, triPrdP1, triPrdP2, triPrdP3;
	int i0, i1, i2, i3;
	for (int triId = 0; triId < triList->GetSize(); ++triId)
	{
		neighborList.m_Data.clear();
		int start = triList->m_Data[triId].x;
		int num = triList->m_Data[triId].y;
		m_shs->FindNeighbors(neighborList, triId);
		//printf("neighbors list: ");
		//for (int i = 0; i < neighborList.GetSize(); ++i)
			//printf("%d ", neighborList.m_Data[i]);
		for (int vtxId = start; vtxId < start + num; ++vtxId)
		{
			i0 = indices->m_Data[vtxId];
			vtxPos = posBuffer->m_Data[i0];
			vtxPrdP = prdPBuffer->m_Data[i0];
			for (int nbIdx = 0; nbIdx < neighborList.GetSize(); ++nbIdx)
			{
				int nbtriId = neighborList.m_Data[nbIdx];
				int start = triList->m_Data[nbtriId].x;
				i1 = indices->m_Data[start];
				i2 = indices->m_Data[start + 1];
				i3 = indices->m_Data[start + 2];
				triPos1 = posBuffer->m_Data[i1];
				triPos2 = posBuffer->m_Data[i2];
				triPos3 = posBuffer->m_Data[i3];
				triPrdP1 = prdPBuffer->m_Data[i1];
				triPrdP2 = prdPBuffer->m_Data[i2];
				triPrdP3 = prdPBuffer->m_Data[i3];
				Contact contact;
				if (VFTest(vtxPos, triPos1, triPos2, triPos3, vtxPrdP, triPrdP1, triPrdP2, triPrdP3, contact, i0, i1, i2, i3))
				{
					contactData.ctxs.m_Data.push_back(contact);
					contactData.ctxStartNum.m_Data.push_back(make_int2(contactData.ctxIndices.GetSize(), 4));
					contactData.ctxIndices.m_Data.push_back(i0);
					contactData.ctxIndices.m_Data.push_back(i1);
					contactData.ctxIndices.m_Data.push_back(i2);
					contactData.ctxIndices.m_Data.push_back(i3);
				}
			}
		}
	}
}

void CollisionSolver::ColliWithShpGrd()
{
	auto posBuffer = &(m_pbdObj->meshTopol.posBuffer);
	auto prdPBuffer = &(m_pbdObj->constrPBDBuffer.prdPBuffer);
	for (int vtxId = 0; vtxId < posBuffer->GetSize(); ++vtxId)
	{
		//point collide with sphere
		bool isCollideSphere = ColliderSphere(prdPBuffer->m_Data[vtxId], m_sphereCenter, m_sphereRadius);
		if (isCollideSphere) //move the point to the point which intersect with sphere
		{
			float3 moveVector = GenerateMoveVectorSphere(m_sphereCenter, m_sphereRadius, prdPBuffer->m_Data[vtxId]);
			prdPBuffer->m_Data[vtxId] += moveVector;
		}
		//point collide with ground
		bool isCollideGoround = CollideGround(prdPBuffer->m_Data[vtxId], m_groundHeight);
		if (isCollideGoround)
		{
			prdPBuffer->m_Data[vtxId].y = m_groundHeight;
		}
	}
	
}

bool CollisionSolver::ColliderSphere(float3 pointPos, float3 sphereOrigin, float r)
{
	float d = DistanceCCD(pointPos, sphereOrigin);
	if (d - r > 0.001)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool CollisionSolver::CollideGround(float3 pointPos, float groundHeight)
{
	if (pointPos.y - groundHeight < 0.001)
	{
		return true;
	}
	else
	{
		return false;
	}
}

float3 CollisionSolver::GenerateMoveVectorSphere(float3 sphereOrigin, float sphereRadius, float3  p)
{
	float moveDistance = sphereRadius - DistanceCCD(sphereOrigin, p);
	float3 moveDirection = (p - sphereOrigin) / DistanceCCD(sphereOrigin, p);
	float3 moveLength = moveDirection * moveDistance;
	return moveLength;
}

//void CCDTestMain()
//{
//	CCDTest ccdTest;
//	ccdTest.PrepareTestData("D://0310ContinuousCollisionDectection//ccdTestData//3ClothStaticTestRestP.txt",
//											 "D://0310ContinuousCollisionDectection//ccdTestData//3ClothStaticTestPrdP.txt");
//	//ccdTest.PrepareTestData("D://0310ContinuousCollisionDectection//ccdTestData//CCDResolvePos.txt",
//	//										"D://0310ContinuousCollisionDectection//ccdTestData//CCDResolvePrdP.txt");
//	CollisionSolver colliSolver;
//
//	float3 cellSize = make_float3(3.0f, 3.0f, 3.0f);
//	float3 gridCenter = make_float3(0.0f, 0.0f, 0.0f);
//	uint3 gridSize = make_uint3(5, 1, 5);
//
//	// initialize SH
//
//	SpatialHashSystem shs(ccdTest.topol.posBuffer, ccdTest.topol.indices, CPU);
//	shs.SetGridCenter(gridCenter);
//	shs.SetGridSize(gridSize);
//	shs.SetDivision(cellSize);
//	shs.InitSH();
//	shs.UpdateSH(0.0f);
//
//	colliSolver.SetTarget(ccdTest.topol, ccdTest.prdPBuffer);
//	colliSolver.SetThickness(0.03f);

//
//	auto start = chrono::steady_clock::now();
//	clock_t tStart = clock();
//
//	for (int i = 0; i < 4; ++i)
//	{
//		//// collision detection
//		colliSolver.CCD_SH();
//		//colliSolver.CCD_N2();
//		//// collision resolves
//		colliSolver.CollisionResolve();
//	}
//	
//	auto end = chrono::steady_clock::now();
//	auto diff = end - start;
//	cout << chrono::duration <double, milli>(diff).count() << " ms" << endl;
//
//	////printf("contact number: %d", colliSolver.contactData.ctxStartNum.GetSize());
//	//set<int> idList;
//	//for (int i = 0; i < colliSolver.contactData.ctxStartNum.GetSize(); ++i)
//	//{
//	//	int id = colliSolver.contactData.ctxStartNum.m_Data[i].x;
//	//	idList.insert(colliSolver.contactData.ctxIndices.m_Data[id]);
//	//}
//	//set<int>::iterator it;
//	//printf("id: ");
//	//for (it = idList.begin(); it != idList.end(); it++)
//	//{
//	//	printf("%d -- ", *it);
//	//}
//
//	colliSolver.SaveResult();
//}

double CollisionSolver::signed_ee_distance(const float3& x0, const float3& x1,
	const float3& y0, const float3& y1,
	float3* n, double* w)
{
	float3 _n; if (!n) n = &_n;
	double _w[4]; if (!w) w = _w;
	*n = cross(normalize(x1 - x0), normalize(y1 - y0));
	if (norm2(*n) < 1e-6)
		return infinity;
	*n = normalize(*n);
	double h = dot(x0 - y0, *n);
	double a0 = stp(y1 - x1, y0 - x1, *n), a1 = stp(y0 - x0, y1 - x0, *n),
		b0 = stp(x0 - y1, x1 - y1, *n), b1 = stp(x1 - y0, x0 - y0, *n);
	w[0] = a0 / (a0 + a1);
	w[1] = a1 / (a0 + a1);
	w[2] = -b0 / (b0 + b1);
	w[3] = -b1 / (b0 + b1);
	return h;
}

void CollisionSolver::SaveResult()
{
	auto m_topol = &(m_pbdObj->meshTopol);
	//printf("------229 vtxPrd: (%f, %f, %f)--------\n", m_prdPBuffer.m_Data[229].x, m_prdPBuffer.m_Data[229].y, m_prdPBuffer.m_Data[229].z);
	m_topol->indices.SetName("Indices");
	m_topol->posBuffer.SetName("P");
	m_pbdObj->constrPBDBuffer.prdPBuffer.SetName("P");
	m_topol->primList.SetName("primList");
	m_topol->posBuffer = m_pbdObj->constrPBDBuffer.prdPBuffer;
	//IO::SaveToplogy(m_topol, "D://0310ContinuousCollisionDectection//ccdTestData//3clothCCDTestTopol.cache");
	//IO::SaveBuffer(m_prdPBuffer, "D://0310ContinuousCollisionDectection//ccdTestData//prdP.cache");
	cout << "topol saved" << endl;
}

//----------------------- CCDTest ----------------------------

void CCDTest::PrepareTestData(string topolPath, string prdPath)
{
	readMeshFromTxt(topolPath);
	readBufferFromTxt(prdPath);
	createPrimList(3);
	cout << topol.indices.GetSize() << endl;
	cout << topol.primList.GetSize() << endl;
}

void CCDTest::readMeshFromTxt(string filename)
{
	std::fstream in;
	in.open(filename, std::ios::in);
	if (!in.is_open()) {
		printf("Error opening the file\n");
		exit(1);
	}

	std::string buffer;
	int lineNum = 0;
	while (std::getline(in, buffer)) {
		// string to char *
		const std::string firstSplit = ":";
		const std::string secondSplit = ",";
		std::string dataLine = splitString(buffer, firstSplit)[1];
		auto dataStringList = splitString(dataLine, secondSplit);
		if (lineNum == 0)  // first line of the input file: position
		{
			assert(dataStringList.size() % 3 == 0);
			for (uint i = 0; i < dataStringList.size(); i += 3)
			{
				// std::cout << dataStringList[i] << std::endl;
				float3 vertexPos = make_float3(std::stof(dataStringList[i]),
					std::stof(dataStringList[i + 1]),
					std::stof(dataStringList[i + 2]));
				// printf("vec %d: %f, %f, %f\n", i/3, vertexPos[0], vertexPos[1], vertexPos[2]);
				topol.posBuffer.m_Data.push_back(vertexPos);
			}
			std::cout << "topol.posBuffer: " << topol.posBuffer.GetSize() << endl;
			assert(topol.posBuffer.GetSize() == dataStringList.size() / 3);
		}
		else  // second line of the input file: vertices tet
		{
			assert(dataStringList.size() % 3 == 0);
			for (uint i = 0; i < dataStringList.size(); i += 3)
			{
				topol.indices.m_Data.push_back(std::stoi(dataStringList[i]));
				topol.indices.m_Data.push_back(std::stoi(dataStringList[i + 1]));
				topol.indices.m_Data.push_back(std::stoi(dataStringList[i + 2]));
			}
		}
		++lineNum;
	}
	in.close();
	printf("Read File Done!\n");
}

void CCDTest::readBufferFromTxt(string filename)
{
	std::fstream in;
	in.open(filename, std::ios::in);
	if (!in.is_open()) {
		printf("Error opening the file\n");
		exit(1);
	}

	std::string buffer;
	int lineNum = 0;
	while (std::getline(in, buffer)) {
		// string to char *
		const std::string firstSplit = ":";
		const std::string secondSplit = ",";
		std::string dataLine = splitString(buffer, firstSplit)[1];
		auto dataStringList = splitString(dataLine, secondSplit);
		if (lineNum == 0)  // first line of the input file: position
		{
			assert(dataStringList.size() % 3 == 0);
			for (uint i = 0; i < dataStringList.size(); i += 3)
			{
				// std::cout << dataStringList[i] << std::endl;
				float3 vertexPos = make_float3(std::stof(dataStringList[i]),
					std::stof(dataStringList[i + 1]),
					std::stof(dataStringList[i + 2]));
				// printf("vec %d: %f, %f, %f\n", i/3, vertexPos[0], vertexPos[1], vertexPos[2]);
				prdPBuffer.m_Data.push_back(vertexPos);
			}
			std::cout << "topol.posBuffer: " << prdPBuffer.GetSize() << endl;
			assert(prdPBuffer.GetSize() == dataStringList.size() / 3);
		}
	}
	in.close();
	printf("Read File Done!\n");
}

void CCDTest::splitString(const std::string& src, std::vector<std::string>& v, const std::string& split)
{
	std::string::size_type pos1, pos2;
	pos2 = src.find(split);
	pos1 = 0;
	while (std::string::npos != pos2)
	{
		v.push_back(src.substr(pos1, pos2 - pos1));
		pos1 = pos2 + split.size();
		pos2 = src.find(split, pos1);
	}
	if (pos1 != src.length())
		v.push_back(src.substr(pos1));
}

std::vector<std::string> CCDTest::splitString(const std::string& src, const std::string& split)
{
	std::vector<std::string> _ret = std::vector<std::string>();
	splitString(src, _ret, split);
	return _ret;
}

void CCDTest::createPrimList(int count)
{
	for (int i = 0; i < topol.indices.GetSize() / count; ++i)
	{
		int2 p;
		p.x = i * count;
		p.y = count;
		topol.primList.m_Data.push_back(p);
	}
}


// DoVF(dt)
/*{
	prdBuffer = posBuffer + t * v;    //calculate four points position after time t
	解三次方程，取最小非负实根
	float e = thickness/edgeAverage;    // calculate epsilon
	if( t < dt )
		将t带入方程求解w1, w2
		if( -e < w1 < 1+e && -e < w2 < 1+e)
			w3 = 1 - w1 - w2
			if( -e < w1 < 1+e)
				colliPrimList.push_back(make_int2(colliIndices.GetSize(), 4))
				colliIndices.push_back(当前点idx，以及当前collide三角形三个点idx)
				colliType.push_back(VF);
}*/

// DoEE(tolerance)
/*{
	计算两条边向量
	if(两条边平行)
		continue;
	解方程求a和b，计算两个交点
	if(a<1 && b < 1) //两个点都在线段上
		d = Distance(a1, a2)
		if( d < tolerance )
			colliPrimList.push_back(make_int2(colliIndices.GetSize(), 4))
			colliIndices.push_back(四个点的idx)
			colliType.push_back(EE);
	else if()
}*/




	