import python_example
python_example.main()
print("completed main")
engine = python_example.get_engine()
# engine.set_as_classes()
ann = python_example.Announcement("1.2.", [1], 1, 1, None, None, python_example.Relationships.ORIGIN, False, True, [])
engine.setup([ann])
engine.run(0);
