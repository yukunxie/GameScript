# Verify operator benchmark checksum
fn main() {
    let floorDiv = 3;
    let modResult = 2;
    let powerResult = 1024;
    let andResult = 4;
    let orResult = 13;
    let xorResult = 9;
    let notResult = -6;
    let leftShift = 12;
    let rightShift = 4;
    let logicalAnd = 1;
    let logicalOr = 1;
    let inList = 1;
    let notInList = 1;
    let inDict = 1;
    let inTuple = 1;
    let precedence = 11;
    let complexExpr = 20;
    let loopSum = 555000;
    
    let checksum = floorDiv + modResult + powerResult + andResult + orResult 
                 + xorResult + leftShift + rightShift + logicalAnd + logicalOr 
                 + inList + notInList + inDict + inTuple + precedence 
                 + complexExpr + loopSum + notResult;
    
    print("Calculated checksum:", checksum);
    return 0;
}
