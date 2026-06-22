#include "pch.h"
#include "MeshSimplicator.h"
#include "MeshStoreBox.h"
#include "ModelParser.h"
#include "Logger.h"
#include <time.h>
#include <windows.h>
#include <unordered_map>

MeshSimplicator::MeshSimplicator()
{
	this->parser = new ModelParser();
	this->storeBox = nullptr;
	this->simplicateLevel = 5;	
	this->simplicateRate = 10;
	this->boundaryWeight = 100.0f;
	this->coord = CoordinateFlag::Source;
	this->fileName = "";
	this->filePath = "";
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}

MeshSimplicator::~MeshSimplicator()
{
	delete this->parser;
	delete this->storeBox;
}

bool MeshSimplicator::LoadModel(std::string path)
{	
	if (parser == nullptr)
		parser = new ModelParser();	

	time_t start = time(NULL);

	MeshStoreBox* box =  parser->LoadModel(path);
	if (box == nullptr)
		return false;

	float weldEpsilon = 1e-6;
	int welded = box->WeldVerticesByPosition(weldEpsilon);
	LOG_INFO("Welding: {} duplicate vertices merged", welded);


	LOG_INFO("Load Model takes {} secondes",(double)(time(NULL) - start));

	// split file name
	auto iter = path.begin();
	while (iter != path.end() && '.' != *iter)
	{
		if (*iter == '/' || *iter == '\\')
		{
			this->filePath += fileName + *iter;
			this->fileName = "";
		}
		else 
		{
			this->fileName += *iter;
		}
		iter++;
	}	

	storeBox = box;

	storeBox->InitElements();
	LOG_INFO("InitElements is Done!");

	// QEM 초기화 로직 수행
	ComputeVertexQMatrices();
	LOG_INFO("ComputeVertexQMatrices is Done!");	
	AddBoundaryConstraints();
	
	GenerateInitialPairs();	
	LOG_INFO("GenerateInitialPairs is Done!");
	
	if (!pairsQueue.empty())
	{
		LOG_INFO("Lowest pair's cost is {}",pairsQueue.top().cost);
	}

	return true;
}

void MeshSimplicator::AddBoundaryConstraints()
{
	using namespace DirectX;

	const auto& faces = storeBox->GetFaces();
	for (QEMFace* f : faces)
	{
		if (f->isDeleted) continue;

		// 면의 법선 (plane.xyz). ComputePlane에서 degenerate면 (0,0,0,0)이므로 건너뜀
		if (f->plane.x == 0.0f && f->plane.y == 0.0f && f->plane.z == 0.0f) continue;

		XMVECTOR nFace = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&f->plane));

		// 세 엣지 (v0-v1, v1-v2, v2-v0) 순회
		for (int i = 0; i < 3; ++i)
		{
			QEMVertex* va = f->indices[i];
			QEMVertex* vb = f->indices[(i + 1) % 3];

			// 중복 처리 방지: 포인터 정렬로 한 번만
			if (!(va < vb)) continue;
			if (va->isDeleted || vb->isDeleted) continue;

			// 공유 면 카운트 (va와 vb가 동시에 들고 있는 면 개수)
			int shared = 0;
			//QEMFace* sharedFaces[2] = { nullptr, nullptr };

			const auto& smaller = (va->adjacentFaces.size() < vb->adjacentFaces.size())
				? va->adjacentFaces : vb->adjacentFaces;
			const auto& larger = (va->adjacentFaces.size() < vb->adjacentFaces.size())
				? vb->adjacentFaces : va->adjacentFaces;

			for (auto* af : smaller)
			{
				if (af->isDeleted) continue;
				if (larger.count(af) > 0)
				{
					//if (shared < 2) sharedFaces[shared] = af; // 포인터 저장
					if (++shared > 2) break;  // 2개 이상이면 내부 엣지
				}
			}

			//bool isBoundary = (shared == 1);
			//bool isUVSeam = false;
			//
			//if (shared == 2 && sharedFaces[0] != nullptr && sharedFaces[1] != nullptr)
			//{
			//	isUVSeam = CheckIfUVSeam(va, vb, sharedFaces[0], sharedFaces[1]);
			//}
			//
			//// 경계도 아니고 Seam도 아니면(즉, 완전히 매끄러운 내부 메쉬면) 가상 평면 생성 안 함
			//if (!isBoundary && !isUVSeam) continue;

			if (shared != 1) continue;  // 경계 엣지가 아님





			// --- 가상 평면 구성 ---
			XMVECTOR pa = XMLoadFloat4(&va->position);
			XMVECTOR pb = XMLoadFloat4(&vb->position);
			XMVECTOR edgeDir = XMVectorSubtract(pb, pa);
			XMVECTOR edgeDirN = XMVector3Normalize(edgeDir);

			XMVECTOR nPerp = XMVector3Normalize(XMVector3Cross(edgeDirN, nFace));

			// degenerate (엣지 길이 0, 또는 엣지가 면 법선과 평행 — 정상 메시에선 안 생김) 방어
			if (XMVector3Equal(nPerp, XMVectorZero()) ||
				std::isnan(XMVectorGetX(nPerp)))
				continue;

			// 평면의 d 계수: -(n · pa)
			float dCoef = -XMVectorGetX(XMVector3Dot(nPerp, pa));

			XMFLOAT3 nStore;
			XMStoreFloat3(&nStore, nPerp);
			float a = nStore.x, b = nStore.y, c = nStore.z, d = dCoef;

			// 가중치 적용된 Kp = w * p * pᵀ
			XMMATRIX Kp = XMMatrixSet(
				boundaryWeight * a * a, boundaryWeight * a * b, boundaryWeight * a * c, boundaryWeight * a * d,
				boundaryWeight * a * b, boundaryWeight * b * b, boundaryWeight * b * c, boundaryWeight * b * d,
				boundaryWeight * a * c, boundaryWeight * b * c, boundaryWeight * c * c, boundaryWeight * c * d,
				boundaryWeight * a * d, boundaryWeight * b * d, boundaryWeight * c * d, boundaryWeight * d * d
			);

			// 양 끝점 Q에 합산
			XMMATRIX qa = XMLoadFloat4x4(&va->Q);
			XMStoreFloat4x4(&va->Q, qa + Kp);

			XMMATRIX qb = XMLoadFloat4x4(&vb->Q);
			XMStoreFloat4x4(&vb->Q, qb + Kp);
		}
	}
}

void MeshSimplicator::Simplicate()
{	
	try
	{
		
		for (int level = 0; level < simplicateLevel; ++level)
		{	
			int pairSize = this->pairsQueue.size();
			int cntOfCollapse = (pairSize / 100) * this->simplicateRate;
            int targetAmount = cntOfCollapse;
            int collapsedCount = 0;

			if (!pairsQueue.empty()) 
			{
				LOG_INFO("Simplicate level is {} , pairs left is {}, collapseCnt is {}", level, pairsQueue.size(), targetAmount);
				LOG_INFO("Lowest pair's cost is {}", pairsQueue.top().cost);
			}

			// 루프 조건 수정 (사이즈가 줄어들더라도 정확히 cntOfCollapse 만큼 실행)
			while (!pairsQueue.empty() && collapsedCount < targetAmount)
			{
				
				QEMPair targetPair = pairsQueue.top();				
				pairsQueue.pop();

				//LOG_INFO(("Simplicate :: Pair Selection :: \n cost : {} \n\t| v1 Info | ptr : {} | \t | v2 Info | ptr : {} |"), 
				//	targetPair.cost, (int)targetPair.v1, (int)targetPair.v2);


				// 이미 지워진 정점이 포함된 Pair라면 건너뛰기
				if (targetPair.v1->isDeleted || targetPair.v2->isDeleted) continue;				

				// face inversion 방지 
				DirectX::XMFLOAT4 safePos;
				if (!TryGetSafePosition(targetPair, safePos)) {					
					continue;  
				}
				targetPair.optimalPos = safePos;

				DirectX::XMFLOAT4 originPosition = targetPair.v1->position;
				targetPair.v1->position = targetPair.optimalPos;				

				// Q 행렬 병합 
				DirectX::XMMATRIX q1 = XMLoadFloat4x4(&targetPair.v1->Q);
				DirectX::XMMATRIX q2 = XMLoadFloat4x4(&targetPair.v2->Q);
				XMStoreFloat4x4(&targetPair.v1->Q, q1 + q2);


				auto facesCopy = targetPair.v2->adjacentFaces;

				for (auto t : facesCopy)
				{
					int idx1 = -1, idx2 = -1;
					for (int k = 0; k < 3; ++k) {
						if (t->indices[k] == targetPair.v1) idx1 = k;
						if (t->indices[k] == targetPair.v2) idx2 = k;
					}

					// Shared Face는 여기서 메모리/관계 정리 (파괴되는 면)
					if (idx1 != -1 && idx2 != -1)
					{
						for (auto v : t->indices) {
							v->adjacentFaces.erase(t);
						}
						t->isDeleted = true;
						continue;
					}

					// 살아남는 면(Surviving Face)에 대해 무게중심 좌표계(Barycentric) UV 보간
					// t에는 v2가 포함되어 있고 v1은 없는 상태입니다.
					if (idx2 != -1)
					{
						using namespace DirectX;

						// 1. 삼각형의 원본 세 정점 3D 위치 (교체되기 전)
						XMVECTOR A = XMLoadFloat4(&t->indices[0]->position);
						XMVECTOR B = XMLoadFloat4(&t->indices[1]->position);
						XMVECTOR C = XMLoadFloat4(&t->indices[2]->position);
						XMVECTOR P = XMLoadFloat4(&targetPair.optimalPos); // 병합될 새로운 3D 위치

						// 2. 기저 벡터 및 내적 계산
						XMVECTOR v0 = XMVectorSubtract(B, A);
						XMVECTOR v1 = XMVectorSubtract(C, A);
						XMVECTOR v2 = XMVectorSubtract(P, A);

						float d00, d01, d11, d20, d21;
						XMStoreFloat(&d00, XMVector3Dot(v0, v0));
						XMStoreFloat(&d01, XMVector3Dot(v0, v1));
						XMStoreFloat(&d11, XMVector3Dot(v1, v1));
						XMStoreFloat(&d20, XMVector3Dot(v2, v0));
						XMStoreFloat(&d21, XMVector3Dot(v2, v1));

						float denom = d00 * d11 - d01 * d01;
						float w0 = 0.0f, w1 = 0.0f, w2 = 0.0f;

						// 3. 가중치 계산 (분모 0에 수렴하는 Degenerate 방어)
						if (std::abs(denom) > 1e-6f)
						{
							w1 = (d11 * d20 - d01 * d21) / denom;
							w2 = (d00 * d21 - d01 * d20) / denom;
							w0 = 1.0f - w1 - w2;
						}
						else
						{
							// 면적이 0에 가까우면 가중치 보간을 포기하고 기존 v2의 UV를 그대로 유지
							if (idx2 == 0) w0 = 1.0f;
							else if (idx2 == 1) w1 = 1.0f;
							else w2 = 1.0f;
						}

						// 4. v2 코너의 모든 UV 채널 갱신
						for (auto iter = t->uvs[idx2].begin(); iter != t->uvs[idx2].end(); iter++)
						{
							int ch = iter->first;

							// 원본 면의 A, B, C 코너에서 해당 채널의 UV 획득 
							// (만약 채널 매핑이 누락된 비정상 코너라면 현재 코너의 UV로 Fallback)
							auto getUV = [&](int cornerIdx) -> DirectX::XMFLOAT2 {
								auto it = t->uvs[cornerIdx].find(ch);
								if (it != t->uvs[cornerIdx].end())
									return DirectX::XMFLOAT2(it->second.u, it->second.v);
								return DirectX::XMFLOAT2(iter->second.u, iter->second.v);
								};

							DirectX::XMFLOAT2 uvA = getUV(0);
							DirectX::XMFLOAT2 uvB = getUV(1);
							DirectX::XMFLOAT2 uvC = getUV(2);

							// 무게중심 가중치를 적용한 정밀 3D-to-2D 보간
							iter->second.u = w0 * uvA.x + w1 * uvB.x + w2 * uvC.x;
							iter->second.v = w0 * uvA.y + w1 * uvB.y + w2 * uvC.y;
						}

						// 보간이 끝났으므로 실제 정점 포인터를 v2에서 v1으로 교체
						t->indices[idx2] = targetPair.v1;
					}

					// 축약 후 중복이 생기면 degenerate → 파괴 처리
					if (t->indices[0] == t->indices[1] ||
						t->indices[1] == t->indices[2] ||
						t->indices[0] == t->indices[2])
					{
						for (auto v : t->indices) v->adjacentFaces.erase(t);
						t->isDeleted = true;
						continue;
					}
					targetPair.v1->adjacentFaces.insert(t);
				}

				// 정점 연결 정보 교체 (여기서도 안전을 위해 복사본 순회)
				auto verticesCopy = targetPair.v2->adjacentVertices;
				for (auto v : verticesCopy)
				{
					if (v == targetPair.v1) continue;

					targetPair.v1->adjacentVertices.insert(v);
					v->adjacentVertices.erase(targetPair.v2);
					v->adjacentVertices.insert(targetPair.v1);
				}
				targetPair.v1->adjacentVertices.erase(targetPair.v2); // ← 추가

				// v2 처리가 끝났으므로 삭제 마킹
				targetPair.v2->isDeleted = true;
				collapsedCount++;
			}
			
			this->storeBox->RemoveDeletedElements();
			LOG_INFO("Level {} RemoveDeletedElements done", level);
			
			std::string exportName = fileName + std::to_string(level) + ".FBX";
			this->parser->ExportModel(this->storeBox, exportName,this->coord, this->filePath);
			LOG_INFO("Level {} ExportModel done", level);

			this->storeBox->ComputePlanes();
			LOG_INFO("Level {} ComputePlanes done", level);

			ComputeVertexQMatrices();
			LOG_INFO("Level {} ComputeVertexQMatrices done", level);

			AddBoundaryConstraints();

			GenerateInitialPairs();
			LOG_INFO("Level {} GenerateInitialPairs done", level);

		}				
		
		
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("Error occurred! \n \t Exception : {}",e.what());		
	}	



}

void MeshSimplicator::SetCoordinate(CoordinateFlag flag)
{
	this->coord = flag;
}

void MeshSimplicator::Reset()
{
	if (storeBox != nullptr)
		delete this->storeBox;	
	delete this->parser;

	this->parser = new ModelParser();
	this->storeBox = nullptr;
	this->simplicateLevel = 5;
	this->simplicateRate = 10;
	this->boundaryWeight = 100.0f;
	this->coord = CoordinateFlag::Source;
	this->fileName = "";
	this->filePath = "";
	
}

bool MeshSimplicator::CheckIfUVSeam(QEMVertex* va, QEMVertex* vb, QEMFace* f1, QEMFace* f2)
{
	// 1. f1과 f2에서 va, vb가 각각 몇 번째 코너(0, 1, 2)인지 찾기
	auto getLocalIdx = [](QEMFace* f, QEMVertex* v) -> int {
		for (int i = 0; i < 3; ++i) {
			if (f->indices[i] == v) return i;
		}
		return -1;
		};

	int va_f1_idx = getLocalIdx(f1, va);
	int va_f2_idx = getLocalIdx(f2, va);
	int vb_f1_idx = getLocalIdx(f1, vb);
	int vb_f2_idx = getLocalIdx(f2, vb);

	// 정상적인 위상이라면 -1이 나올 수 없으나 방어 코드 추가
	if (va_f1_idx == -1 || va_f2_idx == -1 || vb_f1_idx == -1 || vb_f2_idx == -1)
		return false;

	// 2. UV 값이 다른지 비교하는 람다 함수 (채널 고려)
	auto isUVDifferent = [](const std::map<int, UVElement>& uvMap1, const std::map<int, UVElement>& uvMap2) -> bool {
		if (uvMap1.size() != uvMap2.size()) return true;

		for (const auto& pair : uvMap1) {
			int ch = pair.first;
			if (uvMap2.find(ch) == uvMap2.end()) return true; // f2에 해당 채널이 없으면 Seam

			const UVElement& e1 = pair.second;
			const UVElement& e2 = uvMap2.at(ch);

			// 허용 오차(Epsilon) 비교. ExportModel에서 1e-3f 단위 양자화를 하셨으므로 
			// 그에 준하는 미세한 거리 차이 이상이면 분리된 UV(Seam)로 간주합니다.
			float du = e1.u - e2.u;
			float dv = e1.v - e2.v;
			if ((du * du + dv * dv) > 1e-6f) {
				return true;
			}
		}
		return false;
		};

	// 3. va 또는 vb 둘 중 하나라도 양쪽 면에서의 UV가 다르다면 이 간선은 Seam입니다.
	bool isVaSeam = isUVDifferent(f1->uvs[va_f1_idx], f2->uvs[va_f2_idx]);
	bool isVbSeam = isUVDifferent(f1->uvs[vb_f1_idx], f2->uvs[vb_f2_idx]);

	return (isVaSeam || isVbSeam);
}

void MeshSimplicator::ComputeVertexQMatrices()
{
	auto& vertices = storeBox->GetVertices();
	for (auto* vertex : vertices)
	{
		if (vertex->isDeleted) continue;
		// Q 행렬 0으로 초기화		
		DirectX::XMMATRIX vertexQ = DirectX::XMMatrixSet(
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0);

		for (auto* face : vertex->adjacentFaces)
		{
			if (face->isDeleted) continue;
			// 평면 방정식 (a, b, c, d)
			float a = face->plane.x;
			float b = face->plane.y;
			float c = face->plane.z;
			float d = face->plane.w;
			
			// Kp = p * p^T
			DirectX::XMMATRIX Kp = DirectX::XMMatrixSet(
				a*a, a*b, a*c, a*d,
				a*b, b*b, b*c, b*d,
				a*c, b*c, c*c, c*d,
				a*d, b*d, c*d, d*d
			);

			

			vertexQ = vertexQ + Kp;
		}
		
		DirectX::XMStoreFloat4x4(&vertex->Q, vertexQ);
	}
}

/// <summary>
/// 4x4 매트릭스를 디버깅 합니다. 
/// </summary>
/// <param name="Mat"></param>
void LOG_MATRIX(DirectX::XMMATRIX& Mat) {
	LOG_INFO(" Matrix LOG:: Q : \n {} {} {} {} \n {} {} {} {} \n {} {} {} {} \n {} {} {} {}",
		Mat.r[0].m128_f32[0], Mat.r[0].m128_f32[1], Mat.r[0].m128_f32[2], Mat.r[0].m128_f32[3],
		Mat.r[1].m128_f32[0], Mat.r[1].m128_f32[1], Mat.r[1].m128_f32[2], Mat.r[1].m128_f32[3],
		Mat.r[2].m128_f32[0], Mat.r[2].m128_f32[1], Mat.r[2].m128_f32[2], Mat.r[2].m128_f32[3],
		Mat.r[3].m128_f32[0], Mat.r[3].m128_f32[1], Mat.r[3].m128_f32[2], Mat.r[3].m128_f32[3]
	);
}

void MeshSimplicator::ComputePairCostAndPos(QEMPair& p)
{
	using namespace DirectX;

	XMMATRIX q1 = XMLoadFloat4x4(&p.v1->Q);
	XMMATRIX q2 = XMLoadFloat4x4(&p.v2->Q);
	XMMATRIX Q_bar = q1 + q2;

	// 최적화 대상 행렬 M 제작 
	// (Q_bar의 상단 3x3 부분과 마지막 행 [0, 0, 0, 1] 결합)
	XMFLOAT4X4 m_data;
	XMStoreFloat4x4(&m_data, Q_bar);
	m_data._41 = 0.0f;
	m_data._42 = 0.0f;
	m_data._43 = 0.0f;
	m_data._44 = 1.0f;
	
	XMMATRIX M = XMLoadFloat4x4(&m_data);
	
	// M 역행렬 도출 시도
	XMVECTOR det;
	XMMATRIX M_inv = XMMatrixInverse(&det, M);
	
	float detVal = XMVectorGetX(det);
	bool isSingular = std::abs(detVal) < 1e-5f || std::isinf(detVal) || std::isnan(detVal);

	// 결과 cost를 0 이상으로 clamp (음수는 수치 노이즈)
	XMVECTOR pos;
	
	auto computeCost = [&](XMVECTOR v) -> double {
		// v^T * Q_bar * v
		v = XMVectorSetW(v, 1.0f);
		XMMATRIX Q_bar_T = XMMatrixTranspose(Q_bar);
		XMVECTOR Q_bar_v = XMVector4Transform(v, Q_bar_T);
		XMVECTOR vT_Q_bar_v = XMVector4Dot(v, Q_bar_v);
		return static_cast<double>(XMVectorGetX(vT_Q_bar_v));
	};

	if (!isSingular)
	{
		// 역행렬이 존재하면 최적 위치 = M_inv * [0,0,0,1]^T 
		// (본질적으로 M_inv의 4번째 열)
		XMFLOAT4X4 inv_data;
		XMStoreFloat4x4(&inv_data, M_inv);
		pos = XMVectorSet(inv_data._14, inv_data._24, inv_data._34, 1.0f);
	}
	else
	{
		// Fallback 전략: v1, v2, 그 중간점 중 에러가 제일 적은 곳 선택
		XMVECTOR v1pos = XMLoadFloat4(&p.v1->position);
		XMVECTOR v2pos = XMLoadFloat4(&p.v2->position);

		v1pos = XMVectorSetW(v1pos, 1.0f);
		v2pos = XMVectorSetW(v2pos, 1.0f);

		XMVECTOR midpos = XMVectorScale(XMVectorAdd(v1pos, v2pos), 0.5f);
		
		// 스케일 이후 w성분이 0.5f가 되므로 다시 1.0f로 강제 세팅 (동차좌표 대응)
		midpos = XMVectorSetW(midpos, 1.0f);
		
		double err1 = computeCost(v1pos);
		double err2 = computeCost(v2pos);
		double err3 = computeCost(midpos);

		double minErr = min(err1, min(err2, err3));
		if (minErr == err1) pos = v1pos;
		else if (minErr == err2) pos = v2pos;
		else pos = midpos;
	}

	XMStoreFloat4(&p.optimalPos, pos);
	p.cost = max(0.0, computeCost(pos));
	//p.cost = computeCost(pos);
}

bool MeshSimplicator::IsCollapseValid(QEMVertex* v1, QEMVertex* v2, const DirectX::XMFLOAT4& newPos, float minDot)
{
	using namespace DirectX;
	XMVECTOR newP = XMLoadFloat4(&newPos);

	// v1과 v2의 모든 인접 면 검사 (양쪽 모두)
	auto checkFaces = [&](const std::unordered_set<QEMFace*>& faceSet) -> bool
		{
			for (auto* f : faceSet) {
				if (f->isDeleted) continue;

				// 이 face가 v1과 v2를 모두 포함하면 collapse로 파괴되는 face → 스킵
				bool hasV1 = (f->indices[0] == v1 || f->indices[1] == v1 || f->indices[2] == v1);
				bool hasV2 = (f->indices[0] == v2 || f->indices[1] == v2 || f->indices[2] == v2);
				if (hasV1 && hasV2) continue;

				// 기존 normal (face->plane.xyz, 이미 정규화돼 있음)
				XMVECTOR oldN = XMVectorSet(f->plane.x, f->plane.y, f->plane.z, 0.0f);

				// 새 위치 적용한 임시 정점 좌표 3개
				XMFLOAT4 pos[3];
				for (int i = 0; i < 3; ++i) {
					if (f->indices[i] == v1 || f->indices[i] == v2)
						pos[i] = newPos;                    // collapse 결과 위치
					else
						pos[i] = f->indices[i]->position;
				}

				// 새 normal 계산
				XMVECTOR p0 = XMLoadFloat4(&pos[0]);
				XMVECTOR p1 = XMLoadFloat4(&pos[1]);
				XMVECTOR p2 = XMLoadFloat4(&pos[2]);
				XMVECTOR e1 = XMVectorSubtract(p1, p0);
				XMVECTOR e2 = XMVectorSubtract(p2, p0);
				XMVECTOR cross = XMVector3Cross(e1, e2);

				// 면적이 너무 작으면 (degenerate) 거부
				float area2 = XMVectorGetX(XMVector3LengthSq(cross));
				if (area2 < 1e-12f) return false;

				XMVECTOR newN = XMVector3Normalize(cross);

				// dot product 비교
				float dot = XMVectorGetX(XMVector3Dot(oldN, newN));
				if (dot < minDot) return false;             // 반전 또는 과도 회전
			}
			return true;
		};

	return checkFaces(v1->adjacentFaces)
		&& checkFaces(v2->adjacentFaces);
}

bool MeshSimplicator::TryGetSafePosition(QEMPair& p, DirectX::XMFLOAT4& outPos)
{
	using namespace DirectX;

	// 후보 위치들 (cost 작은 순서)
	std::vector<XMFLOAT4> candidates;
	candidates.reserve(4);

	candidates.push_back(p.optimalPos);                          // 1순위: optimal

	XMFLOAT4 mid;                                                 // 2순위: midpoint
	mid.x = (p.v1->position.x + p.v2->position.x) * 0.5f;
	mid.y = (p.v1->position.y + p.v2->position.y) * 0.5f;
	mid.z = (p.v1->position.z + p.v2->position.z) * 0.5f;
	mid.w = 1.0f;
	candidates.push_back(mid);

	candidates.push_back(p.v1->position);                        // 3순위: v1 유지
	candidates.push_back(p.v2->position);                        // 4순위: v2 위치

	// 첫 번째로 valid한 후보 채택
	for (const auto& pos : candidates) {
		if (IsCollapseValid(p.v1, p.v2, pos, /*minDot=*/0.0f)) {
			outPos = pos;
			return true;
		}
	}

	return false;   // 모든 후보 실패 → collapse 자체를 포기
}


void MeshSimplicator::GenerateInitialPairs()
{
	LOG_INFO("MeshSimplicator::GenerateInitialPairs()");

	// 큐 초기화 (swap 트릭)
	std::priority_queue<QEMPair, std::vector<QEMPair>, std::greater<QEMPair>> empty_queue;
	std::swap(pairsQueue, empty_queue);

	auto& vertices = storeBox->GetVertices();
	for (auto* v1 : vertices)
	{
		if (v1->isDeleted) continue;
		//std::cout << "v1 : ( " << v1->position.x << " , " << v1->position.y << " , " << v1->position.z << " ) connected : "<< v1->adjacentVertices.size() << std::endl;
		for (auto* v2 : v1->adjacentVertices)
		{
			// v1 < v2인 경우만 계산하여 중복(v2->v1) 처리 방지
			if (v1 < v2)
			{				
				if (v2->isDeleted) continue;
				QEMPair cp;
				cp.v1 = v1;
				cp.v2 = v2;
				ComputePairCostAndPos(cp);				
				pairsQueue.push(cp);
			}
		}
	}	

}
