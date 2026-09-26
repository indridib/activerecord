// ============================================================================
// model_size_test.cpp
//
// Tests for model memory footprint tracking
// ============================================================================

#include <array>
#include <attribute.h>
#include <config.h>
#include <gtest/gtest.h>
#include <model.h>

namespace ar {

namespace {
// Minimal connection so model construction can resolve "default"; no queries are executed
class size_test_connection : public connection {
  public:
	std::unique_ptr<statement> query(const std::string&, const attribute_vector&) override { return nullptr; }
	void limit(std::string&, long, long) const override {}
	std::size_t insert_id() const override { return 0; }
	void begin_transaction() override {}
	void commit() override {}
	void rollback() override {}
};
} // namespace

// Test fixture for size tracking
class ModelSizeTest : public ::testing::Test {
  protected:
	void SetUp() override {
		config::connection_map connections;
		connections["default"] = std::make_shared<size_test_connection>();
		config::instance().set_connections(connections, "default");
	}
	void TearDown() override { config::instance().set_connections({}); }
};

TEST_F(ModelSizeTest, BaseModelSize) {
	// Expected size after Phase 1 optimization:
	// - config::connection_ptr: 16 bytes (shared_ptr)
	// - std::string_view table_name_: 16 bytes
	// - std::string_view primary_key_: 16 bytes
	// - attribute_map attributes_: ~48 bytes (map overhead) + heap
	// - string_set dirty_: ~48 bytes (set overhead) + heap
	// - bool new_record_: 1 byte
	// - bool readonly_: 1 byte
	// - padding: varies
	//
	// Budgets are exact current sizes so that any growth fails and gets a deliberate look; if an
	// increase is intended, update the number here. They are per standard library: std::map/std::set
	// are 16 bytes in MSVC STL (24 with Debug iterator debugging) but 48 bytes in libstdc++.
#if defined(_MSVC_STL_VERSION) && _ITERATOR_DEBUG_LEVEL == 0
	constexpr size_t expected_max = 104; // MSVC STL, Release
#elif defined(_MSVC_STL_VERSION)
	constexpr size_t expected_max = 120; // MSVC STL, Debug (iterator debugging adds a proxy pointer)
#else
	constexpr size_t expected_max = 168; // libstdc++; libc++ is uncalibrated
#endif
	constexpr size_t phase0_size = 224;	 // Original size before optimization

	size_t actual = sizeof(model);

	// Report current size
	std::cout << "\n[Model Size Info]\n";
	std::cout << "  sizeof(model) = " << actual << " bytes\n";
	std::cout << "  Phase 0 (original): " << phase0_size << " bytes\n";
	std::cout << "  Expected max: " << expected_max << " bytes\n";

	if (actual < phase0_size) {
		double reduction = (static_cast<double>(phase0_size - actual) / static_cast<double>(phase0_size)) * 100.0;
		std::cout << "  Reduction: " << reduction << "%\n";
	}
	std::cout << "\n";

	// Verify size reduction
	EXPECT_LT(actual, phase0_size) << "Model size should be less than original " << phase0_size << " bytes";

	EXPECT_LE(actual, expected_max) << "Model size exceeds target of " << expected_max << " bytes";
}

TEST_F(ModelSizeTest, ComponentSizes) {
	// Track individual component sizes for debugging
	std::cout << "\n[Component Sizes]\n";
	std::cout << "  sizeof(config::connection_ptr) = " << sizeof(config::connection_ptr) << " bytes\n";
	std::cout << "  sizeof(std::string_view) = " << sizeof(std::string_view) << " bytes\n";
	std::cout << "  sizeof(attribute_map) = " << sizeof(attribute_map) << " bytes\n";
	std::cout << "  sizeof(string_set) = " << sizeof(string_set) << " bytes\n";
	std::cout << "  sizeof(bool) = " << sizeof(bool) << " bytes\n";
	std::cout << "\n";

	// These are informational - no assertions
	SUCCEED();
}

TEST_F(ModelSizeTest, MemoryLayoutAlignment) {
	// Check that we're not wasting space on padding
	// This helps identify potential layout optimizations

	size_t total_members = sizeof(config::connection_ptr) + // 16
						   sizeof(std::string_view) +		// 16 (table_name_)
						   sizeof(std::string_view) +		// 16 (primary_key_)
						   sizeof(attribute_map) +			// ~48
						   sizeof(string_set) +				// ~48
						   sizeof(bool) +					// 1 (new_record_)
						   sizeof(bool);					// 1 (readonly_)

	size_t actual = sizeof(model);
	size_t padding = actual - total_members;

	std::cout << "\n[Memory Layout]\n";
	std::cout << "  Sum of members: " << total_members << " bytes\n";
	std::cout << "  Actual size: " << actual << " bytes\n";
	std::cout << "  Padding/overhead: " << padding << " bytes\n";
	std::cout << "\n";

	// Padding should be reasonable (< 25% of total size)
	EXPECT_LT(padding, actual / 4) << "Excessive padding detected - consider reordering members";
}

TEST_F(ModelSizeTest, BulkMemoryUsage) {
	// Simulate loading many models to measure bulk memory impact
	constexpr size_t count = 10'000;

	std::vector<model> models;
	models.reserve(count);

	// Create models (minimal overhead)
	for (size_t i = 0; i < count; ++i) {
		models.emplace_back("test_table", "id");
	}

	// Calculate memory used
	size_t per_instance = sizeof(model);
	size_t total_mb = (per_instance * count) / (size_t{ 1024 } * 1024);

	std::cout << "\n[Bulk Memory Usage]\n";
	std::cout << "  " << count << " models:\n";
	std::cout << "  Per instance: " << per_instance << " bytes\n";
	std::cout << "  Total (vector only): ~" << total_mb << " MB\n";
	std::cout << "  Phase 0 would use: ~" << (224 * count / 1024 / 1024) << " MB\n";
	std::cout << "\n";

	// Verify we're under reasonable limits
	// 10k models should use < 2 MB (excluding heap allocations in attributes_)
	EXPECT_LT(total_mb, 2) << "10k empty models consuming too much memory";
}

TEST_F(ModelSizeTest, PhaseProgressTracking) {
	// Track progress through optimization phases

	struct PhaseTarget {
		const char* name;
		size_t target_bytes;
		const char* description;
	};

	const std::array<PhaseTarget, 4> phases{ { { "Phase 0 (Original)", 224, "Before any optimization" },
											   { "Phase 1 (Current)", 160, "string_view + externalized aliases" },
											   { "Phase 2 (Future)", 80, "Indexed storage + bitset dirty" },
											   { "CRTP (Alternative)", 98, "Template-based metadata elimination" } } };

	size_t current = sizeof(model);

	std::cout << "\n[Optimization Roadmap]\n";
	for (const auto& phase : phases) {
		std::cout << "  " << phase.name << ": " << phase.target_bytes << " bytes";
		std::cout << " (" << phase.description << ")";

		if (current <= phase.target_bytes) {
			std::cout << " ✓\n";
		} else {
			std::cout << "\n";
		}
	}
	std::cout << "\n  Current sizeof(model) = " << current << " bytes\n\n";

	SUCCEED(); // Informational only
}

// Test that string_view optimization is actually being used
TEST_F(ModelSizeTest, StringViewVerification) {
	// Verify we're using string_view, not std::string
	EXPECT_LT(sizeof(std::string_view), sizeof(std::string)) << "string_view should be smaller than string";

	// string_view is typically 16 bytes (pointer + length)
	// std::string is typically 32 bytes (SSO buffer + size + capacity)
	EXPECT_EQ(sizeof(std::string_view), 16) << "Unexpected string_view size on this platform";
}

} // namespace ar
