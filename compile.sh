BLUE='\033[0;34m'
NC='\033[0m'
DATE=$date

clear
echo -e "\n${BLUE} -- Starting compilation $(DATE) --${NC}"

make config=release -j

echo -e "\n${BLUE} -- Compiled $(DATE) --${NC}"