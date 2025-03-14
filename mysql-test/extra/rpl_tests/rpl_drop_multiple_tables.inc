# ==== Purpose ====
#
# Auxiliary test script used in rpl_gtid_split_statements.
#
# The purpose is to test DROP TABLE with multiple tables, each of
# different type. 
#
# ==== Usage ====
#
# CREATE PROCEDURE create_tables () BEGIN ... END;
# CREATE PROCEDURE drop_tables () BEGIN ... END;
# --let $table_list= TABLE_LIST
# --let $remaining_table_list= TABLE_LIST
# --let $automatic= [0|1]
# --let $transaction_count= N
# --source include/rpl_gtid_drop_multiple_tables.inc
#
# Context:
#
#   create_tables
#     This stored procedure will be called in the beginning of the
#     execution. The purpose is to create the tables that will later
#     be dropped.
#
#   drop_tables
#     This stored procedure will be called in the end of the
#     execution.  The purpose is to drop any remaining tables.  This
#     is useful in the case where tables fail to be dropped on the
#     slave.
# 
# Parameters:
#
#   $table_list
#     Comma-separated list of tables to drop. The tables should be
#     created by create_tables; their names should be base, temp_t, or
#     temp_n.
#
#   $remaining_table_list
#     Comma-separated list of tables that were created by
#     create_tables, which are not included in $table_list.
#
#   $automatic
#     If this is 1, uses GTID_NEXT='AUTOMATIC'.  Otherwise, sets
#     GTID_NEXT='UUID:NUMBER' (if gtid_mode=on) or
#     GTID_NEXT='ANONYMOUS' (if gtid_mode=off).
#
#   $transaction_count
#     The number of transactions that the DROP statement is expected
#     to generate.

# Create all tables
CALL create_tables();

# Reset gtid_step_* state.
--source include/gtid_step_reset.inc

# Get offset of events
--let $binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $binlog_position= query_get_value(SHOW BINARY LOG STATUS, Position, 1)

# Set GTID_NEXT.
--let $expect_error= 0
if (!$automatic)
{
  --source include/set_gtid_next_gtid_mode_agnostic.inc
  if ($gtid_mode_on)
  {
    --let $expect_error= 1
    --let $transaction_count= 0
  }
}

# Execute DROP
if ($expect_error)
{
  --error ER_GTID_UNSAFE_BINLOG_SPLITTABLE_STATEMENT_AND_ASSIGNED_GTID
  eval DROP TABLE $table_list;
}
if (!$expect_error)
{
  eval DROP TABLE $table_list;
}

# Revert GTID_NEXT.
if (!$automatic)
{
  SET GTID_NEXT = 'AUTOMATIC';
}

# Assert that gtid_executed was stepped.
--let $gtid_step_count= $transaction_count
--source include/gtid_step_assert.inc

# Assert that events were written to binlog
if (!$expect_error)
{
  if ($gtid_mode_on)
  {
    --let $event_sequence= (Gtid # !Q(DROP.*) # ){$transaction_count}
  }
  if (!$gtid_mode_on)
  {
    --let $event_sequence= (Anonymous_Gtid # !Q(DROP.*) # ){$transaction_count}
  }
}
if ($expect_error)
{
  --let $event_sequence= ()
}
--let $position= $binlog_position
--let $file= $binlog_file
--let $dont_print_pattern= 1
--source include/assert_binlog_events.inc

# Drop remaining tables.
if ($remaining_table_list)
{
  eval DROP TABLE $remaining_table_list; 
}
if ($expect_error)
{
  eval DROP TABLE $table_list;
}

--source include/rpl_sync.inc

#CALL drop_tables();
--source include/rpl_reset.inc
