
import ctypes

# Load the library
lib = ctypes.CDLL('./python_example.so')

# Define the factory function's argument and return types
lib.Announcement_new.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int), ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_bool, ctypes.c_bool, ctypes.c_bool, ctypes.c_int, ctypes.c_bool, ctypes.c_int, ctypes.c_bool, ctypes.c_bool, ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]
lib.Announcement_new.restype = ctypes.c_void_p

# Define the delete function's argument type
lib.Announcement_delete.argtypes = [ctypes.c_void_p]

# Initialize an Announcement object
as_path = (ctypes.c_int * 2)(123, 456)
communities = (ctypes.c_char_p * 1)(b"community1")
ann = lib.Announcement_new(b"192.168.0.0/24", as_path, 2, 1617187200, 0, False, False, False, 0, False, 1, False, False, communities, 1)

print(ann)
# Memory management mistake: not releasing the memory
# Correct approach: Call the delete function when done with the object
lib.Announcement_delete(ann)
