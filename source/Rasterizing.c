static float CalcStepSize(float diff)
{
	if (diff == 0.0f) return 0.0f;
	return fabs(1.0f / diff);
}

static float FindFirstCrossing(float beg, float diff, float stepSize)
{
	if (stepSize == 0.0f) return 9.9f; // return anything >= 1.0f
	if (diff > 0.0f) {
		return stepSize * (ceilf(beg) - beg);
	} else {
		return stepSize * (beg - floorf(beg));
	}
}

static void FlushWrites(Workspace * restrict ws)
{
	for (int i = 0; i < ws->dwbCount; ++i) {
		DotWrite write = ws->dwb[i];
		ws->raster[write.idx / 8].tailValues[write.idx % 8] += write.tailValue;
		ws->raster[write.idx / 8].edgeValues[write.idx % 8] += write.edgeValue;
	}
	ws->dwbCount = 0;
}

static void RasterizeDot(
	Workspace * restrict ws,
	long qbx, long qby, long qex, long qey)
{
	if (ws->dwbCount == 256) {
		FlushWrites(ws);
	}

	// pixel coordinates
	long px = min(qbx, qex) / 1024;
	long py = min(qby, qey) / 1024;

	SKR_assert(px >= 0 && px < ws->dims.width);
	SKR_assert(py >= 0 && py < ws->dims.height);

	DotWrite write;

	long width = (ws->dims.width + 7) / 8; // in cells
	write.idx = 8 * width * py + px;

	long windingAndCover = -(qex - qbx); // winding * cover
	long area = gabs(qey - qby) / 2 + 1024 - (max(qey, qby) - py * 1024);

	write.tailValue = windingAndCover;
	write.edgeValue = windingAndCover * area / 1024;

	ws->dwb[ws->dwbCount++] = write;
}

#define QUANTIZE(x) ((long) ((x) * 1024.0f + 0.5f))

/*
RasterizeLine() is intended to take in a single line and pass it on as a sequence of dots.
Its algorithm is actually fairly simple: It computes the exact intervals at
which the line crosses a horizontal or vertical pixel edge respectively, and
orders them based on the variable scalar in the line equation.
*/

static void RasterizeLine(Workspace * restrict ws, Line line)
{
	float dx = line.end.x - line.beg.x;
	float dy = line.end.y - line.beg.y;

	// step size along each axis
	float sx = CalcStepSize(dx);
	float sy = CalcStepSize(dy);
	// t of next vertical / horizontal intersection
	float xt = FindFirstCrossing(line.beg.x, dx, sx);
	float yt = FindFirstCrossing(line.beg.y, dy, sy);

	long prev_qx = QUANTIZE(line.beg.x);
	long prev_qy = QUANTIZE(line.beg.y);

	while (xt < 1.0f || yt < 1.0f) {
		float t;
		if (xt < yt) {
			t = xt;
			xt += sx;
		} else {
			t = yt;
			yt += sy;
		}

		long qx = QUANTIZE(line.beg.x + t * dx);
		long qy = QUANTIZE(line.beg.y + t * dy);

		RasterizeDot(ws, prev_qx, prev_qy, qx, qy);

		prev_qx = qx;
		prev_qy = qy;
	}

	long qx = QUANTIZE(line.end.x);
	long qy = QUANTIZE(line.end.y);

	RasterizeDot(ws, prev_qx, prev_qy, qx, qy);
}

static void DrawLine(Workspace * restrict ws, Line line)
{
	RasterizeLine(ws, line);
}

// TODO take monitor gamma i guess?
static double CalcLinearToGamma(double L)
{
	double S;
	if (L <= 0.0031308) {
		S = L * 12.92;
	} else {
		S = 1.055 * pow(L, 1.0 / 2.2) - 0.055;
	}
	return S;
}

static unsigned char LinearToGamma[1025];

void skrInitializeLibrary(void)
{
	for (int i = 0; i <= 1024; ++i) {
		LinearToGamma[i] = round(CalcLinearToGamma(i / 1024.0) * 255.0);
	}
}

unsigned long skrCalcCellCount(SKR_Dimensions dims)
{
	return (dims.width + 7) / 8 * dims.height;
}

void skrCastImage(
	RasterCell const * restrict source,
	unsigned char * restrict dest,
	SKR_Dimensions dims)
{
	long width = (dims.width + 7) / 8; // in cells

	__m128i c1024 = _mm_set1_epi16(1024);
	__m128i c255 = _mm_set1_epi16(255);

	for (long col = 0; col < width; ++col) {

		__m128i accumulators = _mm_setzero_si128();

		for (long row = 0; row < dims.height; ++row) {

			long cellIdx = width * row + col;
			__m128i edgeValues = _mm_loadu_si128(
				(__m128i const *) source[cellIdx].edgeValues);
			__m128i tailValues = _mm_loadu_si128(
				(__m128i const *) source[cellIdx].tailValues);

			__m128i values = _mm_adds_epi16(accumulators, edgeValues);
			__m128i linearValues = _mm_min_epi16(_mm_max_epi16(values,
				_mm_setzero_si128()), c1024);
			// TODO gamma correction
			__m128i gammaValues = _mm_srai_epi16(linearValues, 2);
			gammaValues = _mm_min_epi16(gammaValues, c255);
			__m128i compactValues = _mm_packus_epi16(gammaValues, _mm_setzero_si128());

			accumulators = _mm_adds_epi16(accumulators, tailValues);

			int togo = dims.width - col * 8;
			__attribute__((aligned(8))) char pixels[8];
			_mm_storel_epi64((__m128i *) pixels, compactValues);
			memcpy(&dest[dims.width * row + col * 8], pixels, min(togo, 8));
		}
		// TODO assertion
	}
}

#if 0
void skrConvertImage(unsigned char const * restrict source, unsigned char * restrict dest, SKR_Dimensions dims, SKR_Format destFormat))
{
	for (long i = 0; i < dims.width * dims.height; ++i) {
		
	}
}
#endif

