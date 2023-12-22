import python_example
ann = python_example.Announcement("123", [5], 5, 5, False, 5, python_example.Relationships.ORIGIN\
, False, False, [])
engine = python_example.get_engine()
engine.setup([ann])
