import python_example
ann = python_example.Announcement("123", [1,2], 1, 1, False, 1, python_example.Relationships.ORIGIN\
, False, False, [])
engine = python_example.get_engine()
engine.setup([ann])
