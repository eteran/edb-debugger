
#ifndef UTIL_MATH_H_2020227_
#define UTIL_MATH_H_2020227_

namespace util {

//------------------------------------------------------------------------------
// Name: percentage
// Desc: calculates how much of a multi-region byte search we have completed
//------------------------------------------------------------------------------
template <class N1, class N2, class N3, class N4>
int percentage(N1 regions_finished, N2 regions_total, N3 bytes_done, N4 bytes_total) {

	// how much percent does each region account for?
	const auto region_step = 1.0f / static_cast<float>(regions_total) * 100.0f;

	// how many regions are done?
	const float regions_complete = region_step * regions_finished;

	// how much of the current region is done?
	const float region_percent = region_step * static_cast<float>(bytes_done) / static_cast<float>(bytes_total);

	return static_cast<int>(regions_complete + region_percent);
}

//------------------------------------------------------------------------------
// Name: percentage
// Desc: calculates how much of a single-region byte search we have completed
//------------------------------------------------------------------------------
template <class N1, class N2>
int percentage(N1 bytes_done, N2 bytes_total) {
	return percentage(0, 1, bytes_done, bytes_total);
}

}

#endif
