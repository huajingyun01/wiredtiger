/*-
 * Public Domain 2014-present MongoDB, Inc.
 * Public Domain 2008-2014 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <algorithm>
#include <iostream>

#include "model/kv_table.h"
#include "wiredtiger.h"

namespace model {

/*
 * kv_table::contains_any --
 *     Check whether the table contains the given key-value pair. If there are multiple values
 *     associated with the given timestamp, return true if any of them match.
 */
bool
kv_table::contains_any(const data_value &key, const data_value &value, timestamp_t timestamp) const
{
    const kv_table_item *item = item_if_exists(key);
    if (item == nullptr)
        return false;
    return item->contains_any(value, fix_timestamp(timestamp));
}

/*
 * kv_table::contains_any --
 *     Check whether the table contains the given key-value pair. If there are multiple values
 *     associated with the given timestamp, return true if any of them match.
 */
bool
kv_table::contains_any(kv_checkpoint_ptr ckpt, const data_value &key, const data_value &value) const
{
    const kv_table_item *item = item_if_exists(key);
    if (item == nullptr)
        return false;
    return item->contains_any(ckpt, value);
}

/*
 * kv_table::get --
 *     Get the value. Return a copy of the value if is found, or NONE if not found. Throw an
 *     exception on error.
 */
data_value
kv_table::get(const data_value &key, timestamp_t timestamp) const
{
    const kv_table_item *item = item_if_exists(key);
    if (item == nullptr)
        return NONE;
    return item->get(fix_timestamp(timestamp));
}

/*
 * kv_table::get --
 *     Get the value. Return a copy of the value if is found, or NONE if not found. Throw an
 *     exception on error.
 */
data_value
kv_table::get(kv_checkpoint_ptr ckpt, const data_value &key, timestamp_t timestamp) const
{
    const kv_table_item *item = item_if_exists(key);
    if (item == nullptr)
        return NONE;
    return item->get(ckpt, fix_timestamp(timestamp));
}

/*
 * kv_table::get --
 *     Get the value. Return a copy of the value if is found, or NONE if not found. Throw an
 *     exception on error.
 */
data_value
kv_table::get(kv_transaction_ptr txn, const data_value &key) const
{
    const kv_table_item *item = item_if_exists(key);
    if (item == nullptr)
        return NONE;
    return timestamped() ? item->get(txn) : item->get_latest(txn);
}

/*
 * kv_table::get_ext --
 *     Get the value and return the error code instead of throwing an exception.
 */
int
kv_table::get_ext(const data_value &key, data_value &out, timestamp_t timestamp) const
{
    try {
        out = get(key, fix_timestamp(timestamp));
        return out == NONE ? WT_NOTFOUND : 0;
    } catch (wiredtiger_exception &e) {
        out = NONE;
        return e.error();
    }
}

/*
 * kv_table::get_ext --
 *     Get the value and return the error code instead of throwing an exception.
 */
int
kv_table::get_ext(
  kv_checkpoint_ptr ckpt, const data_value &key, data_value &out, timestamp_t timestamp) const
{
    try {
        out = get(ckpt, key, fix_timestamp(timestamp));
        return out == NONE ? WT_NOTFOUND : 0;
    } catch (wiredtiger_exception &e) {
        out = NONE;
        return e.error();
    }
}

/*
 * kv_table::get_ext --
 *     Get the value and return the error code instead of throwing an exception.
 */
int
kv_table::get_ext(kv_transaction_ptr txn, const data_value &key, data_value &out) const
{
    try {
        out = get(txn, key);
        return out == NONE ? WT_NOTFOUND : 0;
    } catch (wiredtiger_exception &e) {
        out = NONE;
        return e.error();
    }
}

/*
 * kv_table::insert --
 *     Insert into the table (non-transactional API).
 */
int
kv_table::insert(
  const data_value &key, const data_value &value, timestamp_t timestamp, bool overwrite)
{
    try {
        item(key).add_update(
          std::move(kv_update(value, fix_timestamp(timestamp))), false, !overwrite);
        return 0;
    } catch (wiredtiger_exception &e) {
        return e.error();
    }
}

/*
 * kv_table::insert --
 *     Insert into the table.
 */
int
kv_table::insert(
  kv_transaction_ptr txn, const data_value &key, const data_value &value, bool overwrite)
{
    std::shared_ptr<kv_update> update = fix_timestamps(std::make_shared<kv_update>(value, txn));
    try {
        item(key).add_update(update, false, !overwrite);
        txn->add_update(*this, key, update);
        return 0;
    } catch (wiredtiger_exception &e) {
        return e.error();
    }
}

/*
 * kv_table::remove --
 *     Delete a value from the table (non-transactional API).
 */
int
kv_table::remove(const data_value &key, timestamp_t timestamp)
{
    kv_table_item *item = item_if_exists(key);
    if (item == nullptr)
        return WT_NOTFOUND;
    try {
        item->add_update(std::move(kv_update(NONE, fix_timestamp(timestamp))), true, false);
        return 0;
    } catch (wiredtiger_exception &e) {
        return e.error();
    }
}

/*
 * kv_table::remove --
 *     Delete a value from the table.
 */
int
kv_table::remove(kv_transaction_ptr txn, const data_value &key)
{
    kv_table_item *item = item_if_exists(key);
    if (item == nullptr)
        return WT_NOTFOUND;

    std::shared_ptr<kv_update> update = fix_timestamps(std::make_shared<kv_update>(NONE, txn));
    try {
        item->add_update(update, true, false);
        txn->add_update(*this, key, update);
        return 0;
    } catch (wiredtiger_exception &e) {
        return e.error();
    }
}

/*
 * kv_table::truncate --
 *     Truncate a key range (non-transactional API).
 */
int
kv_table::truncate(const data_value &start, const data_value &stop, timestamp_t timestamp)
{
    std::lock_guard lock_guard(_lock);
    if (start != model::NONE && stop != model::NONE && start > stop)
        throw model_exception("The start and the stop key are not in the right order");

    auto start_iter = start == model::NONE ? _data.begin() : _data.lower_bound(start);
    auto stop_iter = stop == model::NONE ? _data.end() : _data.upper_bound(stop);

    try {
        for (auto i = start_iter; i != stop_iter; i++)
            i->second.add_update(
              std::move(kv_update(NONE, fix_timestamp(timestamp))), false, false);
    } catch (wiredtiger_exception &e) {
        return e.error();
    }

    return 0;
}

/*
 * kv_table::truncate --
 *     Truncate a key range.
 */
int
kv_table::truncate(kv_transaction_ptr txn, const data_value &start, const data_value &stop)
{
    std::lock_guard lock_guard(_lock);
    if (start != model::NONE && stop != model::NONE && start > stop)
        throw model_exception("The start and the stop key are not in the right order");

    auto start_iter = start == model::NONE ? _data.begin() : _data.lower_bound(start);
    auto stop_iter = stop == model::NONE ? _data.end() : _data.upper_bound(stop);

    try {
        for (auto i = start_iter; i != stop_iter; i++) {
            std::shared_ptr<kv_update> update =
              fix_timestamps(std::make_shared<kv_update>(NONE, txn));
            i->second.add_update(update, false, false);
            txn->add_update(*this, i->first, update);
        }
    } catch (wiredtiger_exception &e) {
        return e.error();
    }

    return 0;
}

/*
 * kv_table::update --
 *     Update a key in the table (non-transactional API).
 */
int
kv_table::update(
  const data_value &key, const data_value &value, timestamp_t timestamp, bool overwrite)
{
    try {
        item(key).add_update(
          std::move(kv_update(value, fix_timestamp(timestamp))), !overwrite, false);
        return 0;
    } catch (wiredtiger_exception &e) {
        return e.error();
    }
}

/*
 * kv_table::update --
 *     Update a key in the table.
 */
int
kv_table::update(
  kv_transaction_ptr txn, const data_value &key, const data_value &value, bool overwrite)
{
    std::shared_ptr<kv_update> update = fix_timestamps(std::make_shared<kv_update>(value, txn));
    try {
        item(key).add_update(update, !overwrite, false);
        txn->add_update(*this, key, update);
        return 0;
    } catch (wiredtiger_exception &e) {
        return e.error();
    }
}

/*
 * kv_table::fix_timestamps --
 *     Fix the commit and durable timestamps for the corresponding update. We need to do this,
 *     because WiredTiger transaction API specifies the commit timestamp after performing the
 *     operations, not before.
 */
void
kv_table::fix_timestamps(const data_value &key, txn_id_t txn_id, timestamp_t commit_timestamp,
  timestamp_t durable_timestamp)
{
    item(key).fix_timestamps(txn_id, commit_timestamp, durable_timestamp);
}

/*
 * kv_table::rollback_updates --
 *     Roll back updates of an aborted transaction.
 */
void
kv_table::rollback_updates(const data_value &key, txn_id_t txn_id)
{
    item(key).rollback_updates(txn_id);
}

/*
 * kv_table::clear --
 *     Clear the contents of the table.
 */
void
kv_table::clear()
{
    std::lock_guard lock_guard(_lock);
    _data.clear();
}

/*
 * kv_table::rollback_to_stable --
 *     Roll back the database table to the latest stable timestamp and transaction snapshot.
 */
void
kv_table::rollback_to_stable(timestamp_t timestamp, kv_transaction_snapshot_ptr snapshot)
{
    std::lock_guard lock_guard(_lock);

    /* RTS works only on timestamped tables. */
    if (!timestamped())
        return;

    for (auto &p : _data)
        p.second.rollback_to_stable(timestamp, snapshot);
}

/*
 * kv_table::verify_cursor --
 *     Create a verification cursor for the table. This method is not thread-safe. In fact, nothing
 *     is thread-safe until the returned cursor stops being used!
 */
kv_table_verify_cursor
kv_table::verify_cursor()
{
    return std::move(kv_table_verify_cursor(_data));
}

} /* namespace model */
