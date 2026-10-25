#include <stdint.h>

typedef void (*Draw)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef void (*Bind)(uint64_t, uint64_t, uint64_t);
typedef uint64_t (*Barrier)(uint64_t);
typedef void (*Descriptors)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t *, uint64_t, const uint32_t *);
typedef void (*Vertices)(uint64_t, uint64_t, uint64_t, const uint64_t *, const uint64_t *);
typedef void (*Colour)(float, float, float, float);

uint64_t raven_entry(Draw draw, Bind bind, Barrier barrier, uint64_t marker, Descriptors descriptors, Vertices vertices, Colour colour)
{
	for (uint64_t index = 0; index < 4096; ++index) {
		if (colour) colour((float)(marker + index) + .25f, -(float)index - .5f, 3.75f, -4.5f);
		draw(marker, index, index + 1, index + 2, index + 3, index + 4);
		if (index % 64 == 0) {
			bind(marker, index + 10, index + 20);
			const uint64_t count = (index / 64) % 6, dynamic_count = (count + 2) % 6;
			uint64_t sets[6], buffers[6], offsets[6];
			uint32_t dynamic[6];
			for (uint64_t item = 0; item < 6; ++item) {
				sets[item] = 0x1234567800000000ULL + marker + index * 100 + item;
				buffers[item] = sets[item] + 20;
				offsets[item] = sets[item] + 40;
				dynamic[item] = 0x87650000U + (uint32_t)(marker + index * 100 + item);
			}
			descriptors(marker, 9, 10, 11, count, count ? sets : 0, dynamic_count, dynamic_count ? dynamic : 0);
			vertices(marker, 12, count, count ? buffers : 0, count ? offsets : 0);
			for (uint64_t item = 0; item < 6; ++item) {
				*(volatile uint64_t *)&sets[item] = 0;
				*(volatile uint64_t *)&buffers[item] = 0;
				*(volatile uint64_t *)&offsets[item] = 0;
				*(volatile uint32_t *)&dynamic[item] = 0;
			}
			if (barrier(marker) != index + 1)
				return 1;
		}
	}
	return barrier(marker) == 4096 ? 0 : 2;
}
