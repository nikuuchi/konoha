// written by kimio kuramitsu

K.import("sugar");

Expr tyCheckTrue(Stmt stmt, Expr expr, Gamma gma, int typeid) {
	return stmt.printError("true was redefined");
}

K.addExprTyCheck("true", tyCheckTrue);
assert(true);


