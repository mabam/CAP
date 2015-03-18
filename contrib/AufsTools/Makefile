all:
	(cd stuffit; make)
	(cd unstuffit; make)
	(cd binhex; make)
	(cd mcvert; make)
	(cd capit; make)

install:
	(cd stuffit; make install)
	(cd unstuffit; make install)
	(cd binhex; make install)
	(cd mcvert; make install)
	(cd capit; make install)
	(cd shell; make install)

clean:
	(cd stuffit; make clean)
	(cd unstuffit; make clean)
	(cd binhex; make clean)
	(cd mcvert; make clean)
	(cd capit; make clean)

reallyclean: clean
	rm -f man/formatted/*.1

spotless: reallyclean
	rm -f *.orig */*.orig

format:
	(cd man; ./makeman)
