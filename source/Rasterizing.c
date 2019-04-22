#define GRAIN_BITS 8
#define GRAIN (1 << GRAIN_BITS)

static __m128i skr_min_epi32(__m128i a, __m128i b)
{
	__m128i mask = _mm_cmplt_epi32(a, b);
	__m128i maskedA = _mm_and_si128(mask, a);
	__m128i maskedB = _mm_andnot_si128(mask, b);
	return _mm_or_si128(maskedA, maskedB);
}

static __m128i Quantize(__m128 value)
{
	__m128 const cGRAIN = _mm_set1_ps(GRAIN);
	__m128 const c0_5 = _mm_set1_ps(0.5f);

	value = _mm_mul_ps(value, cGRAIN);
	value = _mm_add_ps(value, c0_5);
	return _mm_cvtps_epi32(value);
}

static void RasterizeDots(
	Workspace * restrict ws, int count,
	float start[2], float ends[4][2])
{
	__m128 endX = _mm_set_ps(
		ends[3][0], ends[2][0], ends[1][0], ends[0][0]);
	__m128 endY = _mm_set_ps(
		ends[3][1], ends[2][1], ends[1][1], ends[0][1]);

	__m128i qex = Quantize(endX);
	__m128i qey = Quantize(endY);

	uint32_t qbx0 = start[0] * GRAIN + 0.5f;
	uint32_t qby0 = start[1] * GRAIN + 0.5f;
	__m128i qbx = _mm_bslli_si128(qex, 4);
	__m128i qby = _mm_bslli_si128(qey, 4);
	qbx = _mm_or_si128(qbx, _mm_loadu_si32(&qbx0));
	qby = _mm_or_si128(qby, _mm_loadu_si32(&qby0));

	__m128i px = skr_min_epi32(qbx, qex);
	__m128i py = skr_min_epi32(qby, qey);
	px = _mm_srli_epi32(px, GRAIN_BITS);
	py = _mm_srli_epi32(py, GRAIN_BITS);

	// TODO assert for px / py bounds
	
	uint32_t qbxS[4], qbyS[4];
	_mm_storeu_si128((__m128i *) qbxS, qbx);
	_mm_storeu_si128((__m128i *) qbyS, qby);
	uint32_t qexS[4], qeyS[4];
	_mm_storeu_si128((__m128i *) qexS, qex);
	_mm_storeu_si128((__m128i *) qeyS, qey);
	uint32_t pxS[4], pyS[4];
	_mm_storeu_si128((__m128i *) pxS, px);
	_mm_storeu_si128((__m128i *) pyS, py);

	uint32_t cellIdx[4];
	int16_t edgeValue[4], tailValue[4];
	for (int i = 0; i < 4; ++i) {
		int32_t windingAndCover = qbxS[i] - qexS[i];
		uint32_t area = GRAIN;
		area += gabs(qeyS[i] - qbyS[i]) >> 1;
		area -= max(qeyS[i], qbyS[i]);
		area += pyS[i] << GRAIN_BITS;

		cellIdx[i] = ws->rasterWidth * pyS[i] + pxS[i];
		edgeValue[i] = windingAndCover * area / GRAIN;
		tailValue[i] = windingAndCover;
	}

	for (int i = 0; i < count; ++i) {
		uint32_t idx = cellIdx[i];

		RasterCell cell = ws->raster[idx];

		cell.edgeValue += edgeValue[i];
		cell.tailValue += tailValue[i];

		ws->raster[idx] = cell;
	}
}

static float CalcStepSize(float diff)
{
	if (diff == 0.0f) return 0.0f;
	return fabs(1.0f / diff);
}

static float FindFirstCrossing(float beg, float diff, float stepSize)
{
	if (diff == 0.0f) return 9.9f; // return anything >= 1.0f
	if (diff < 0.0f) beg = -beg;
	return stepSize * (ceilf(beg) - beg);
}

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

	int count = 0;
	float start[2], ends[4][2];
	start[0] = line.beg.x;
	start[1] = line.beg.y;

	while (xt < 1.0f || yt < 1.0f) {
		float t;
		if (xt < yt) {
			t = xt;
			xt += sx;
		} else {
			t = yt;
			yt += sy;
		}

		float cur[2];
		cur[0] = line.beg.x + t * dx;
		cur[1] = line.beg.y + t * dy;

		ends[count][0] = cur[0];
		ends[count][1] = cur[1];

		++count;

		if (count > 3) {
			RasterizeDots(ws, count, start, ends);
			start[0] = cur[0];
			start[1] = cur[1];
			count = 0;
		}
	}

	ends[count][0] = line.end.x;
	ends[count][1] = line.end.y;

	++count;

	RasterizeDots(ws, count, start, ends);
}

static void DrawLine(Workspace * restrict ws, Line line)
{
	if (gabs(line.end.x - line.beg.x) >= 1.0f / GRAIN) {
		RasterizeLine(ws, line);
	}
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

static unsigned char LinearToGamma[GRAIN + 1];

void skrInitializeLibrary(void)
{
	for (int i = 0; i <= GRAIN; ++i) {
		LinearToGamma[i] = round(CalcLinearToGamma(i / (float) GRAIN) * 255.0);
	}
}

uint32_t CalcRasterWidth(SKR_Dimensions dims)
{
	return (dims.width + 7) & ~7;
}

// TODO get rid of this function
unsigned long skrCalcCellCount(SKR_Dimensions dims)
{
	return CalcRasterWidth(dims) * dims.height;
}

void skrCastImage(
	RasterCell const * restrict source,
	unsigned char * restrict dest,
	SKR_Dimensions dims)
{
	// TODO read from workspace instead
	long const width = CalcRasterWidth(dims);

	__m128i const lowMask  = _mm_set1_epi32(0x0000FFFF);
	__m128i const highMask = _mm_set1_epi32(0xFFFF0000);
#define SHUFFLE_MASK _MM_SHUFFLE(3, 1, 2, 0)

	for (long col = 0; col < width; col += 8) {

		RasterCell const * restrict cell = source + col;
		unsigned char * restrict pixel = dest + col;

		__m128i accumulators = _mm_setzero_si128();

		for (long i = dims.height; i > 0; --i, cell += width, pixel += dims.width) {

			__m128i cells1 = _mm_loadu_si128(
				(__m128i const *) cell);
			__m128i cells2 = _mm_loadu_si128(
				(__m128i const *) (cell + 4));

			__m128i edgeValues1 = _mm_and_si128(cells1, lowMask);
			__m128i edgeValues2 = _mm_slli_epi32(cells2, 16);
			__m128i edgeValues = _mm_or_si128(edgeValues1, edgeValues2);

			__m128i tailValues1 = _mm_srli_epi32(cells1, 16);
			__m128i tailValues2 = _mm_and_si128(cells2, highMask);
			__m128i tailValues = _mm_or_si128(tailValues1, tailValues2);

			__m128i values = _mm_adds_epi16(accumulators, edgeValues);

			accumulators = _mm_adds_epi16(accumulators, tailValues);
			
			__m128i shuf1 = _mm_shufflelo_epi16(values, SHUFFLE_MASK);
			__m128i shuf2 = _mm_shufflehi_epi16(shuf1, SHUFFLE_MASK);
			__m128i shuf3 = _mm_shuffle_epi32(shuf2, SHUFFLE_MASK);

			__m128i compactValues = _mm_packus_epi16(shuf3, _mm_setzero_si128());
			int togo = dims.width - col;
			__attribute__((aligned(8))) char pixels[8];
			_mm_storel_epi64((__m128i *) pixels, compactValues);
			memcpy(pixel, pixels, min(togo, 8));
		}
		// TODO assertion
	}
#undef SHUFFLE_MASK
}

#if 0
void skrConvertImage(unsigned char const * restrict source, unsigned char * restrict dest, SKR_Dimensions dims, SKR_Format destFormat))
{
	for (long i = 0; i < dims.width * dims.height; ++i) {
		
	}
}
#endif

