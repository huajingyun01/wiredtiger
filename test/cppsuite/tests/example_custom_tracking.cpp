/*
 * Temporary file to show how overriding of workload_tracking is done. To be removed once the
 * skeleton generation script is updated.
 */

#include "test_harness/test.h"

#include "test_harness/connection_manager.h"
#include "test_harness/util/api_const.h"

using namespace test_harness;

class custom_workload_tracking : public test_harness::workload_tracking {

    using workload_tracking::workload_tracking;

    int
    populate_tracking_cursor(const tracking_operation &operation, const uint64_t &collection_id,
      const std::string &key, const std::string &value, wt_timestamp_t ts,
      scoped_cursor &op_track_cursor) override
    {
        /* Insert dummy keys into the tracking table as an example */
        op_track_cursor->set_key(op_track_cursor.get(), 123);
        op_track_cursor->set_value(op_track_cursor.get(), 456);
    }
};

class example_custom_tracking : public test {
    public:
    example_custom_tracking(const test_args &args) : test(args)
    {
        delete this->_workload_tracking;
        this->_workload_tracking =
          new custom_workload_tracking(_config->get_subconfig(WORKLOAD_TRACKING),
            _config->get_bool(COMPRESSION_ENABLED), *_timestamp_manager);
    }

    void
    validate(const std::string &operation_table_name, const std::string &schema_table_name,
      const std::vector<uint64_t> &known_collection_ids) override final
    {
      // Skip the default validation
      // TODO - Add check that tests with custom tracking table can't use default validation
    }
};
