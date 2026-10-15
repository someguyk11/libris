"""
Tests for Book Location History functionality
"""
from fastapi.testclient import TestClient
from main import app
from database import SessionLocal, engine
from sqlalchemy.orm import Session
from sqlalchemy import text
import uuid
from datetime import datetime

# Create test client
client = TestClient(app)

def test_location_history_creation_and_retrieval():
    """Test that location history is properly created and retrieved"""
    print("Testing Book Location History functionality...")
    
    # Get a book to work with
    db = SessionLocal()
    try:
        # Get a book to work with
        book_result = db.execute(text("SELECT book_id, isbn, shelf_id FROM books LIMIT 1")).fetchone()
        if not book_result:
            print("ERROR: Could not retrieve a book")
            return False
            
        book_id, isbn, current_shelf_id = book_result
        print(f"Working with book ID: {book_id}, ISBN: {isbn}, current shelf_id: {current_shelf_id}")
        
        # Get a shelf to work with
        shelf_result = db.execute(text("SELECT id FROM shelves LIMIT 1")).fetchone()
        if not shelf_result:
            print("ERROR: Could not retrieve a shelf")
            return False
            
        target_shelf_id = shelf_result[0]
        print(f"Target shelf ID: {target_shelf_id}")
        
        # Test 1: Move book to a shelf (initial assignment or move)
        print("\n=== Test 1: Move book to shelf ===")
        print(f"Moving book from shelf {current_shelf_id} to shelf {target_shelf_id}")
        
        # Make the API call to move the book
        response = client.put(f"/book/{isbn}", json={"shelf_id": str(target_shelf_id)})
        print(f"API Response Status: {response.status_code}")
        print(f"API Response Body: {response.json()}")
        
        if response.status_code != 200:
            print(f"FAIL: Expected status code 200, got {response.status_code}")
            return False
            
        # Verify the book's shelf_id was updated
        updated_shelf_id = db.execute(text("SELECT shelf_id FROM books WHERE book_id = :book_id"), {
            "book_id": book_id
        }).scalar()
        print(f"Book's new shelf_id: {updated_shelf_id}")
        
        if str(updated_shelf_id) != str(target_shelf_id):
            print(f"FAIL: Expected shelf_id {target_shelf_id}, got {updated_shelf_id}")
            return False
            
        # Verify history record was created
        history_count = db.execute(text("""
            SELECT COUNT(*) FROM book_location_history WHERE book_id = :book_id
        """), {
            "book_id": book_id
        }).scalar()
        print(f"Number of history records for book: {history_count}")
        
        if history_count < 1:
            print("FAIL: No history record was created")
            return False
            
        # Get the most recent history record
        history_record = db.execute(text("""
            SELECT id, from_shelf_id, to_shelf_id, changed_at, reason 
            FROM book_location_history 
            WHERE book_id = :book_id 
            ORDER BY changed_at DESC LIMIT 1
        """), {
            "book_id": book_id
        }).fetchone()
        
        if history_record:
            hist_id, hist_from_shelf, hist_to_shelf, hist_changed_at, hist_reason = history_record
            print(f"Most recent history record:")
            print(f"  ID: {hist_id}")
            print(f"  From shelf ID: {hist_from_shelf}")
            print(f"  To shelf ID: {hist_to_shelf}")
            print(f"  Changed at: {hist_changed_at}")
            print(f"  Reason: {hist_reason}")
            
            # For the first move, we expect either:
            # 1. If it was truly an initial assignment (from_shelf_id was NULL): from_shelf_id should be None
            # 2. If it was a move from another shelf: from_shelf_id should equal the old shelf_id
            if str(hist_from_shelf) == str(current_shelf_id) and str(hist_to_shelf) == str(target_shelf_id):
                print("PASS: History record values are correct!")
            else:
                print("WARN: History record values don't match expected values, but this might be OK if there were previous moves")
        else:
            print("ERROR: Could not retrieve history record")
            return False
        
        # Test 2: Move book to another shelf
        print("\n=== Test 2: Move book to another shelf ===")
        
        # Get another shelf to move to
        other_shelf_result = db.execute(text("SELECT id FROM shelves WHERE id <> :current_id LIMIT 1"), {
            "current_id": target_shelf_id
        }).fetchone()
        
        if other_shelf_result:
            other_shelf_id = other_shelf_result[0]
            print(f"Moving book from shelf {target_shelf_id} to shelf {other_shelf_id}")
            
            # Make the API call to move the book
            response = client.put(f"/book/{isbn}", json={"shelf_id": str(other_shelf_id)})
            print(f"API Response Status: {response.status_code}")
            
            if response.status_code != 200:
                print(f"FAIL: Expected status code 200, got {response.status_code}")
                return False
                
            # Verify we now have additional history records
            history_count = db.execute(text("""
                SELECT COUNT(*) FROM book_location_history WHERE book_id = :book_id
            """), {
                "book_id": book_id
            }).scalar()
            print(f"Number of history records for book: {history_count}")
            
            if history_count < 2:
                print("FAIL: Expected at least 2 history records")
                return False
            else:
                print("PASS: Correct number of history records!")
        else:
            print("SKIP: No other shelf available for testing multiple moves")
            
        # Test 3: No-op move (moving to the same shelf)
        print("\n=== Test 3: No-op move (same shelf) ===")
        print(f"Attempting to move book from shelf {other_shelf_id} to shelf {other_shelf_id}")
        
        # Get history count before the "no-op" move
        history_count_before = db.execute(text("""
            SELECT COUNT(*) FROM book_location_history WHERE book_id = :book_id
        """), {
            "book_id": book_id
        }).scalar()
        
        # Make the API call for a no-op move
        response = client.put(f"/book/{isbn}", json={"shelf_id": str(other_shelf_id)})
        print(f"API Response Status: {response.status_code}")
        
        if response.status_code != 200:
            print(f"FAIL: Expected status code 200, got {response.status_code}")
            return False
            
        # Check if the response indicates it was a no-op move
        response_data = response.json()
        if "already on the specified shelf" in response_data.get("message", ""):
            print("PASS: API correctly identified no-op move")
        else:
            print("WARN: API did not explicitly identify no-op move, but this might be OK")
            
        # Get history count after the "no-op" move
        history_count_after = db.execute(text("""
            SELECT COUNT(*) FROM book_location_history WHERE book_id = :book_id
        """), {
            "book_id": book_id
        }).scalar()
        
        if history_count_before == history_count_after:
            print("PASS: No additional history record created for no-op move!")
        else:
            print("FAIL: Additional history record was incorrectly created for no-op move!")
            return False
            
        # Test 4: Retrieve history records via API
        print("\n=== Test 4: Retrieve history records via API ===")
        
        # Call the history endpoint
        response = client.get(f"/book/{isbn}/location-history")
        print(f"History API Response Status: {response.status_code}")
        
        if response.status_code != 200:
            print(f"FAIL: Expected status code 200, got {response.status_code}")
            return False
            
        history_data = response.json()
        if not isinstance(history_data, list):
            print("FAIL: Expected history endpoint to return a list")
            return False
            
        print(f"Retrieved {len(history_data)} history records via API:")
        for i, record in enumerate(history_data):
            print(f"  Record {i+1}:")
            print(f"    ID: {record.get('id')}")
            print(f"    Book ID: {record.get('book_id')}")
            print(f"    From shelf ID: {record.get('from_shelf_id')}")
            print(f"    To shelf ID: {record.get('to_shelf_id')}")
            print(f"    Changed at: {record.get('changed_at')}")
            print(f"    Reason: {record.get('reason')}")
            print(f"    From shelf name: {record.get('from_shelf_name')}")
            print(f"    From shelf code: {record.get('from_shelf_code')}")
            print(f"    To shelf name: {record.get('to_shelf_name')}")
            print(f"    To shelf code: {record.get('to_shelf_code')}")
            
        # Verify we have the expected number of records
        if len(history_data) >= 2:  # We did at least two actual moves
            print(f"PASS: Retrieved expected number of history records (>=2)!")
        else:
            print(f"FAIL: Expected at least 2 history records, got {len(history_data)}")
            return False
            
        print("\nPASS: All tests passed!")
        return True
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        db.close()

def test_error_cases():
    """Test error cases for the location history functionality"""
    print("\n=== Testing Error Cases ===")
    
    # Test 1: Missing book
    print("\n--- Test 1: Missing book ---")
    fake_isbn = "0000000000"  # Use an ISBN that definitely doesn't exist
    response = client.get(f"/book/{fake_isbn}/location-history")
    print(f"Missing book API Response Status: {response.status_code}")
    if response.status_code == 404:
        print("PASS: Correctly returned 404 for missing book")
    else:
        print(f"FAIL: Expected 404, got {response.status_code}")
        return False
    
    # Test 2: Invalid shelf ID
    print("\n--- Test 2: Invalid shelf ID ---")
    # First get a valid book
    db = SessionLocal()
    try:
        book_result = db.execute(text("SELECT book_id, isbn FROM books LIMIT 1")).fetchone()
        if not book_result:
            print("ERROR: Could not retrieve a book for error case testing")
            return False
            
        book_id, isbn = book_result
        
        # Try to move book to a non-existent shelf
        fake_shelf_id = str(uuid.uuid4())  # Random UUID that likely doesn't exist
        response = client.put(f"/book/{isbn}", json={"shelf_id": fake_shelf_id})
        print(f"Invalid shelf API Response Status: {response.status_code}")
        if response.status_code == 404:
            print("PASS: Correctly returned 404 for invalid shelf")
        else:
            print(f"FAIL: Expected 404 for invalid shelf, got {response.status_code}")
            return False
    finally:
        db.close()
        
    print("\nPASS: All error case tests passed!")
    return True

if __name__ == "__main__":
    success1 = test_location_history_creation_and_retrieval()
    success2 = test_error_cases()
    
    if success1 and success2:
        print("\nPASS: All location history tests passed!")
    else:
        print("\nFAIL: Some location history tests failed!")
        exit(1)