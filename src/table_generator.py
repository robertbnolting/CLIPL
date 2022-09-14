s = ""

for i in range(256):
	if i % 10 == 0:
		s += "\n"
	# Operator
	if i in [33, 42, 43, 45, 47, 58, 60, 61, 62]:
		s += "0020, "
	# Separator
	elif i in [40, 41, 44, 46, 59, 91, 93, 123, 125]:
		s += "0040, "
	# Digits
	elif i in range(48, 58):
		s += "0010, "
	# Upper case letters
	elif i in range(65, 91):
		s += "0002, "
	# Lower case letters
	elif i in range(97, 123):
		s += "0004, "
	else:
		if i < 255:
			s += "0000, "
		else:
			s += "0000"

print(s)
