#include "jfv_cpu.h"

int JFV_CPU::jfvTrianglePointNumber = 0;
int JFV_CPU::jfvBufferWidth = INIT_WINDOW_WIDTH; 
int	JFV_CPU::jfvBufferHeight = INIT_WINDOW_WIDTH;
IntegerPoint* JFV_CPU::jfvBufferA = NULL;
IntegerPoint* JFV_CPU::jfvBufferB = NULL;
bool JFV_CPU::jfvReadingBufferA = true;
vector<IntegerPoint> JFV_CPU::jfvSeedSamples;
vector<float> JFV_CPU::jfvSamplesArea;
IntegerPoint JFV_CPU::jfvTriangle[3];
std::map<int , IntegerPoint*> JFV_CPU::BufferAMap;
std::map<int , IntegerPoint*> JFV_CPU::BufferBMap;

// �ж����ص�x��y�Ƿ�����������
bool JFV_CPU::jfvIsInTriangle(int x, int y)
{
	IntegerPoint P(0, x, y);    
	
	IntegerPoint v0 = jfvTriangle[2] - jfvTriangle[0];
	IntegerPoint v1 = jfvTriangle[1] - jfvTriangle[0];
	IntegerPoint v2 = P - jfvTriangle[0];

	float dot00 = v0.Dot(v0) ;
	float dot01 = v0.Dot(v1) ;
	float dot02 = v0.Dot(v2) ;
	float dot11 = v1.Dot(v1) ;
	float dot12 = v1.Dot(v2) ;

	float inverDeno = 1 / (dot00 * dot11 - dot01 * dot01) ;

	float u = (dot11 * dot02 - dot01 * dot12) * inverDeno ;
	if (u < 0 || u > 1) // if u out of range, return directly
	{
		return false ;
	}

	float v = (dot00 * dot12 - dot01 * dot02) * inverDeno ;
	if (v < 0 || v > 1) // if v out of range, return directly
	{
		return false ;
	}

	return u + v <= 1 ;
}

// �ͷ���Դ
void JFV_CPU::jfvClearBuffers(){
	for(std::map<int, IntegerPoint*>::iterator it = BufferAMap.begin();it != BufferAMap.end(); ++it)
	{
		IntegerPoint* ptrVector = it->second;
		free(ptrVector);
	}
	for(std::map<int, IntegerPoint*>::iterator it = BufferBMap.begin();it != BufferBMap.end(); ++it)
	{
		IntegerPoint* ptrVector = it->second;
		free(ptrVector);
	}
}

// ���༶buffer����Ϊmap�������ظ�ʹ��
void JFV_CPU::jfvMallocBuffers( void ) {		
	if (BufferAMap.find(jfvBufferWidth) == BufferAMap.end())
	{
		BufferAMap[jfvBufferWidth] = (IntegerPoint*)malloc( sizeof( IntegerPoint ) * jfvBufferWidth * jfvBufferWidth );
	}
	if (BufferBMap.find(jfvBufferWidth) == BufferBMap.end())
	{
		BufferBMap[jfvBufferWidth] = (IntegerPoint*)malloc( sizeof( IntegerPoint ) * jfvBufferWidth * jfvBufferWidth );
	}
	jfvBufferA = BufferAMap[jfvBufferWidth];
	jfvBufferB = BufferBMap[jfvBufferWidth];
}
// ����ά������ת��Ϊ��άʱ�����˻�Ϊֱ�ߣ�ѡȡͶ��ķ���
int jfvJudgeNormal(PointFloat3 _trianglePoints[3])
{
	PointFloat3 p01 = _trianglePoints[0] - _trianglePoints[1];
	PointFloat3 p12 = _trianglePoints[1] - _trianglePoints[2];
	
	PointFloat3 crossV = p01.Cross(p12);
	crossV.normalise();

	PointFloat3 dirX(1.0f, 0.0f, 0.0f);
	float dotV = crossV.Dot(dirX);
	// �����������x�ᴹֱ����ô����0
	if (dotV > 0.999999 || dotV < -0.999999)
		return 0;

	PointFloat3 dirY(0.0f, 1.0f, 0.0f);
	dotV = crossV.Dot(dirY);
	// �����������y�ᴹֱ����ô����1
	if (dotV > 0.999999 || dotV < -0.999999)
		return 1;

	// ���򷵻�2
	return 2;
}

// ����ά�ռ両����ת��Ϊbuffer���ص�
IntegerPoint JFV_CPU::jfv2D2Pixel(int mindex, float fx, float fy, float fxMin, float fxMax, float fyMin, float fyMax)
{
	// mindex ��ǰ���ӵ������ţ�fx��fy�Ǹ��������꣬fxMin, fxMax, fyMin, fyMax�ֱ��Ǹ���������ı߽�
	float tX = (fx - fxMin)/(fxMax - fxMin) * jfvBufferWidth;
	float tY = (fy - fyMin)/(fyMax - fyMin) * jfvBufferHeight;
	// תΪint�͵�
	IntegerPoint tempP =  IntegerPoint(mindex, (int)tX, (int)tY);
	return  tempP;
}

// ��ȡ�����������߽�
void getMaxMin(PointFloat3 _trianglePoints[3], float &fxMax, float &fxMin, int indexA)
{
	if (_trianglePoints[0].points[indexA] < _trianglePoints[1].points[indexA])
	{
		fxMin = _trianglePoints[0].points[indexA];
		fxMax = _trianglePoints[1].points[indexA];
	}
	else
	{
		fxMin = _trianglePoints[1].points[indexA];
		fxMax = _trianglePoints[0].points[indexA];
	}

	if (_trianglePoints[2].points[indexA] < fxMin)
	{
		fxMin = _trianglePoints[2].points[indexA];
	}
	else if (_trianglePoints[2].points[indexA] > fxMax)
	{
		fxMax = _trianglePoints[2].points[indexA];
	}

}

// ΪJFV_CPU���������涥��͹�������
void JFV_CPU::jfvSetTriangle(PointFloat3 _trianglePoints[3], vector<PointFloat3> _samplePoints, bool insertAssert)
{
	// �����Ч������ϵ
	int judgeValue = jfvJudgeNormal(_trianglePoints);
	int indexA = (judgeValue+1)%3;
	int indexB = (judgeValue+2)%3;

	// ��ȡ�����α߽�
	float fxMin, fxMax, fyMin, fyMax;
	getMaxMin(_trianglePoints, fxMax, fxMin, indexA);
	getMaxMin(_trianglePoints, fyMax, fyMin, indexB);

	// ��������ת��Ϊ��������ϵ
	for (unsigned int i = 0;i < 3;i ++)
		jfvTriangle[i] = jfv2D2Pixel(0, _trianglePoints[i].points[indexA], _trianglePoints[i].points[indexB], fxMin, fxMax, fyMin, fyMax);
	jfvTrianglePointNumber = 3;

	// ��������ת��Ϊ��������ϵ
	jfvSeedSamples.clear();
	bool *boolBuffer = (bool*)malloc( sizeof( bool ) * jfvBufferWidth * jfvBufferHeight );
	memset(boolBuffer, 0, sizeof( bool ) * jfvBufferWidth * jfvBufferHeight);
	// ��ǰbuffer�Ĵ�С�ѱ�������Ѱ��������ظ�㣬ǿ��д��
	if (insertAssert == true)
	{
		for (unsigned int i = 0;i < _samplePoints.size();i ++)
		{
			IntegerPoint tPoint = jfv2D2Pixel(i, _samplePoints[i].points[indexA], _samplePoints[i].points[indexB], fxMin, fxMax, fyMin, fyMax);
			int tmindex = tPoint.y * jfvBufferWidth + tPoint.x;
			// ���֮ǰ����δ��Ⱦɫ����ô����Ⱦɫ
			if (boolBuffer[tmindex] == false)
			{
				jfvSeedSamples.push_back(tPoint);
				boolBuffer[tmindex] = true;
			}
			else
			{
				int tm_i, tm_j, mmindex, hasgetNumber = 0;
				// ��0��0��ʼ��Ѱ��δ��Ⱦɫ�����ص���Ϊ�����
				for (tm_j = 0;(tm_j < jfvBufferHeight);tm_j ++)
				{
					for (tm_i = 0;(tm_i < jfvBufferWidth);tm_i ++)
					{
						mmindex = tm_j * jfvBufferWidth + tm_i;
						if (boolBuffer[mmindex] == false)
						{
							hasgetNumber = 1;
							boolBuffer[mmindex] = true;
							break;
						}
					}
					if (hasgetNumber==1)
						break;
				}
				tPoint.x = tm_i; tPoint.y = tm_j;
				// ȷ������û�����
				if (tPoint.x == jfvBufferWidth || tPoint.y == jfvBufferHeight)
					assert(0);
				jfvSeedSamples.push_back(tPoint);
			}
		}
		free(boolBuffer);
		return;
	}
	// �����ǰbuffer�Ĵ�С�ɱ䣬����¼���ظ�����
	for (unsigned int i = 0;i < _samplePoints.size();i ++)
	{
		IntegerPoint tPoint = jfv2D2Pixel(i, _samplePoints[i].points[indexA], _samplePoints[i].points[indexB], fxMin, fxMax, fyMin, fyMax);
		int tmindex = tPoint.y * jfvBufferWidth + tPoint.x;
		if (boolBuffer[tmindex] == false)
		{
			jfvSeedSamples.push_back(tPoint);
			boolBuffer[tmindex] = true;
		}
	}
	free(boolBuffer);
}

// Jump Flooding Algorithm
JFV_ERROR JFV_CPU::jfvExecuteJumpFlooding( void ) {

	if (jfvTrianglePointNumber != 3)
		return JFV_ERROR_NO_TRIANGLE_POINTS;
	// No seeds will just give us a black screen :P
	if( jfvSeedSamples.size() < 1 ) {
		printf( "Please create at least 1 seed.\n" );
		return JFV_ERROR_NO_SAMPLE_POINTS;
	}

	//printf( "Executing the Jump Flooding algorithm...\n" );

	// Clear the buffers before we start
	jfvMallocBuffers();
	
	if ( jfvBufferA == NULL || jfvBufferB == NULL )
		return JFV_ERROR_NO_BUFFER_MEMORY;

	// ��ʼ������
	// Initialize BufferA with (-1,-1), indicating an invalid closest seed.
	// We don't need to initialize BufferB because it will be written to in the first round.
	for( int y = 0; y < jfvBufferHeight; ++y ) {
		for( int x = 0; x < jfvBufferWidth; ++x ) {
			int idx = ( y * jfvBufferWidth ) + x;
			jfvBufferA[ idx ].x = -1;
			jfvBufferA[ idx ].y = -1;
			jfvBufferA[ idx ].mIndex = -1;
		}
	}

	jfvReadingBufferA = true;
	//memset(jfvBufferA, 0, sizeof( IntegerPoint ) * jfvBufferWidth * jfvBufferHeight);
	// Put the seeds into the first buffer
	for( unsigned int i = 0; i < jfvSeedSamples.size(); ++i ) {
		IntegerPoint& p = jfvSeedSamples[i];
		int tIndexInBuffer = ( p.y * jfvBufferWidth ) + p.x;
		if (tIndexInBuffer >= jfvBufferHeight * jfvBufferWidth)
			return JFV_ERROR_SAMPLE_POINTS_OUTSIDE;
		if (jfvBufferA[ tIndexInBuffer ].mIndex > 0)
			return JFV_ERROR_TOO_SMALL_BUFFER;
		if (p.mIndex < 0)
			int abc = 1;
		jfvBufferA[ tIndexInBuffer ] = p;
	}
	
	// Initial step length is half the image's size. If the image isn't square,
	// we use the largest dimension.
	int step = jfvBufferWidth > jfvBufferHeight ? jfvBufferWidth/2 : jfvBufferHeight/2;
	
	// We read from the RBuffer and write into the WBuffer
	IntegerPoint* RBuffer;
	IntegerPoint* WBuffer;

	// Carry out the rounds of Jump Flooding
	while( step >= 1 ) {

		// Set which buffers we'll be using
		if( jfvReadingBufferA == true ) {
			RBuffer = jfvBufferA;
			WBuffer = jfvBufferB;
		}
		else {
			RBuffer = jfvBufferB;
			WBuffer = jfvBufferA;
		}

		// Iterate over each point to find its closest seed
		for( int y = 0; y < jfvBufferHeight; ++y ) {
			for( int x = 0; x < jfvBufferWidth; ++x ) {

				// The point's absolute index in the buffer
				int idx = ( y * jfvBufferWidth ) + x;

				// The point's current closest seed (if any)
				IntegerPoint& p = RBuffer[ idx ];

				// Go ahead and write our current closest seed, if any. If we don't do this
				// we might lose this information if we don't update our seed this round.
				WBuffer[ idx ] = p;

				// This is a seed, so skip this point
				if( p.x == x && p.y == y )
					continue;

				// This variable will be used to judge which seed is closest
				float dist;

				if( p.x == -1 || p.y == -1 )
					dist = -1; // No closest seed has been found yet
				else
					dist = float( (p.x-x)*(p.x-x) + (p.y-y)*(p.y-y) ); // Current closest seed's distance

				// To find each point's closest seed, we look at its 8 neighbors thusly:
				//   (x-step,y-step) (x,y-step) (x+step,y-step)
				//   (x-step,y     ) (x,y     ) (x+step,y     )
				//   (x-step,y+step) (x,y+step) (x+step,y+step)

				for( int ky = -1; ky <= 1; ++ky ) {
					for( int kx = -1; kx <= 1; ++kx ) {

						// Calculate neighbor's row and column
						int ny = y + ky * step;
						int nx = x + kx * step;

						// If the neighbor is outside the bounds of the buffer, skip it
						if( nx < 0 || nx >= jfvBufferWidth || ny < 0 || ny >= jfvBufferHeight )
							continue;

						// Calculate neighbor's absolute index
						int nidx = ( ny * jfvBufferWidth ) + nx;

						// Retrieve the neighbor
						IntegerPoint& pk = RBuffer[ nidx ];

						// If the neighbor doesn't have a closest seed yet, skip it
						if( pk.x == -1 || pk.y == -1 )
							continue;

						// Calculate the distance from us to the neighbor's closest seed
						float newDist = float( (pk.x-x)*(pk.x-x) + (pk.y-y)*(pk.y-y) );

						// If dist is -1, it means we have no closest seed, so we might as well take this one
						// Otherwise, only adopt this new seed if it's closer than our current closest seed
						if( dist == -1 || newDist < dist ) {
							if (pk.mIndex < 0)
								int abc = 0;
							WBuffer[ idx ] = pk;
							dist = newDist;
						}

					}
				}

			}
		}

		// Halve the step.
		step /= 2;

		// Swap the buffers for the next round
		jfvReadingBufferA = !jfvReadingBufferA;
	}
	return JFV_NO_ERROR;
}

// ��¼ÿ�����Ӳ�����Ĳ������
void JFV_CPU::jfvCountArea(float totalArea)
{
	int sampleSize = jfvSeedSamples.size();
	jfvSamplesArea.clear();
	for (int i = 0;i < sampleSize;i ++)
		jfvSamplesArea.push_back(0);

	// ���������һ�����ӣ������������������
	if (sampleSize == 1)
	{
		jfvSamplesArea[0] = totalArea;
		return;
	}
	IntegerPoint* Buffer = JFV_CPU::jfvGetDisplayBuffer();
	int triangleCount = 0;
	// ͳ��ÿ�����Ӱ������ص����Ŀ
	for( int y = 0; y < jfvBufferHeight; ++y ) 
	{
		for( int x = 0; x < jfvBufferWidth; ++x ) 
		{
			if (jfvIsInTriangle(x, y))
			{
				int idx = y * JFV_CPU::jfvBufferWidth + x;
				jfvSamplesArea[Buffer[idx].mIndex] ++;
				triangleCount ++;
			}
		}
	}
	// ��������ΰ��������ش���0������������Ŀ��Ӧ�����
	if (triangleCount > 0)
	{
		for (int i = 0;i < sampleSize;i ++)
		{
			jfvSamplesArea[i] *= (totalArea/triangleCount);
			if (jfvSamplesArea[i] == 0)
			{
				assert(1);
			}
		}
	}
}

//void JFV_CPU::test() {
//	int NumCudaDevice = 0 ;
//    cudaGetDeviceCount( & NumCudaDevice ) ;
//}