import os
from sqlalchemy import create_engine, inspect, text
from dotenv import load_dotenv

# Load environment variables from .env file in this directory
dotenv_path = r'C:\Users\admin\OneDrive\Documents\Libris software\libot-v1\LiBot\Back-End\.env'
print('Loading dotenv from:', dotenv_path)
load_dotenv(dotenv_path)
DATABASE_URL = os.getenv('DATABASE_URL')
print('DATABASE_URL:', DATABASE_URL)
if not DATABASE_URL:
    raise RuntimeError("DATABASE_URL not set in .env")

engine = create_engine(DATABASE_URL)
inspector = inspect(engine)

def get_table_info(table_name):
    print(f"\n=== Table: {table_name} ===")
    # Columns
    columns = inspector.get_columns(table_name)
    print("Columns:")
    for col in columns:
        null_str = "NOT NULL" if not col['nullable'] else "NULL"
        default_str = f" DEFAULT {repr(col['default'])}" if col['default'] is not None else ""
        print(f"  - {col['name']}: {col['type']} {null_str}{default_str}")
    # Primary key
    pk = inspector.get_pk_constraint(table_name)
    print(f"Primary Key: {pk['constrained_columns'] if pk['constrained_columns'] else '(none)'}")
    # Foreign keys
    fks = inspector.get_foreign_keys(table_name)
    print("Foreign Keys:")
    if fks:
        for fk in fks:
            print(f"  - {fk['constrained_columns']} -> {fk['referred_table']}({fk['referred_columns']}) "
                  f"{fk['name']}")
    else:
        print("  (none)")
    # Unique constraints
    uqs = inspector.get_unique_constraints(table_name)
    print("Unique Constraints:")
    if uqs:
        for uq in uqs:
            print(f"  - {uq['column_names']} {uq['name']}")
    else:
        print("  (none)")
    # Indexes
    indexes = inspector.get_indexes(table_name)
    print("Indexes:")
    if indexes:
        for idx in indexes:
            uniq = "UNIQUE" if idx['unique'] else ""
            print(f"  - {idx['column_names']} {idx['name']} {uniq}")
    else:
        print("  (none)")
    # Approximate row count (read-only)
    try:
        with engine.connect() as conn:
            result = conn.execute(text(f"SELECT COUNT(*) FROM {table_name}"))
            rowcount = result.scalar()
            print(f"Approximate Row Count: {rowcount}")
    except Exception as e:
        print(f"Row count error: {e}")

# List of tables of interest
tables_of_interest = [
    'books', 'book_copies', 'shelves',
    'navigation_nodes', 'navigation_edges',
    'robot_jobs', 'reservations', 'borrow_records'
]

# Get all tables in schema
all_tables = inspector.get_table_names()
print("All tables in database:", all_tables)

for table in tables_of_interest:
    if table in all_tables:
        get_table_info(table)
    else:
        print(f"\n=== Table: {table} ===")
        print("Table does NOT exist in database.")

# Also show relationships via foreign keys across all tables
print("\n=== Global Foreign Key Relationships ===")
for table in all_tables:
    fks = inspector.get_foreign_keys(table)
    for fk in fks:
        print(f"{table}.{fk['constrained_columns']} -> {fk['referred_table']}.{fk['referred_columns']}")
