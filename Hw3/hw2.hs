module WordTree(WordTree(Word, Subword, Root), emptyTree, getAllPaths, addWords, deleteWords, getWordsBeginWith) where

data WordTree = Word String | Subword String [WordTree] | Root [WordTree]

emptyTree :: WordTree
emptyTree = Root []
-- DO NOT MODIFY ABOVE

getAllPaths :: WordTree -> [[String]]
addWords :: WordTree -> [String] -> WordTree
deleteWords :: WordTree -> [String] -> WordTree
getWordsBeginWith :: WordTree -> String -> [String]
    
instance Show WordTree where
    show a = auxWordTree a 0 ""
    
    
    
    
exampleTree1 = Root [Word "Dam",Word "Plane", Word "Ship"]
exampleTree2 = Root [Subword "Da"[Subword "m" [Word "", Subword "ag" [Word "e", Word "ing" ]],Word "rk"]]
exampleTree3 = Root [Subword "Ca"[Subword "n" [Word "", Word "teen"], Word "ptain", 
                    Subword "r" [Word "",Subword "r" [Subword "ie" [Word "d",Word "s"], Word "y"]]], Subword "He" [Word "ck", Subword "l" [Word "lo",
                    Subword "p" [Word "",Subword "e" [Word "d", Word "r"],Word "ing"]]]]

tree1 = Root [ Word "Hello",Word "World"]
tree2 = Root [ Subword "Hel" [ Word "lo" , Word "p" ]]
tree3 = Root [ Subword "Hel" [ Word "lo" , Word "p" ],Word "World"]

test1 = Root [ Subword "He" [Word "ck",Subword "l" [Word "lo",Subword "p" [Word "", Subword "e" [Word "d", Word "r"], Word "ing"]]]]
test3 = Root [ Subword "F" [Word "alse", Subword "i" [Word "asco",Word "le"]], Subword "Re" [Word "aper",Subword "po" [Word "",Word "sitory"]],
               Subword "T" [Subword "a" [Subword "il" [Word "", Word "or"],Word "p"],Word "esla"]]


auxWordTree (Root [ ]) depth subword= ""
auxWordTree (Root ((Word x):xs)) depth subword = indent(depth) ++ subword++x  ++ "\n"  ++ auxWordTree(Root xs) (depth) (subword)
auxWordTree (Root ((Subword x y):xs)) depth subword= indent (depth) ++ subword ++ x ++ ":\n" ++ auxWordTree(Root y) (depth+1) (subword++x) ++ auxWordTree(Root xs) (depth) (subword)

indent 0 = ""
indent n = "  " ++ indent(n-1)

getAllPaths x = getAllPaths' x []
getAllPaths' (Root [ ]) z = []
getAllPaths' (Root ((Word x):xs)) z = [z++[x]] ++ getAllPaths'(Root xs) z
getAllPaths' (Root ((Subword x y):xs)) z = getAllPaths'(Root y) (z++[x]) ++ getAllPaths'(Root xs) z


addWords (Root x) y = Root (auxAddWords x (sort(y)))

sort :: (Ord a) => [a] -> [a]  
sort [] = []  
sort (x:xs) =  sort( lower ) ++ [x] ++ sort(greater)
    where
        lower = filter(<x) xs
        greater = filter(>=x) xs


auxAddWords::[WordTree]->[String]->[WordTree]
auxAddWords x [] = x
auxAddWords wt (y:ys) = if empty then auxAddWords after_first_push_wordtree ys 
                                 else auxAddWords evaluated_wordtree ys 
                                    where 
                                    evaluated_wordtree = auxAddWords' wt y False
                                    after_first_push_wordtree = [Word y]
                                    subword_will_occur = check wt y
                                    empty = (length(wt) == 0)
                                    
check::[WordTree]->String->Bool
check [] _ = False
check ((Word w):wt) y = if found ([w] ++ [y]) 0 then True else check wt y
check ((Subword x z):xs) y= if found([x] ++ [y]) 0 then True else check xs y

auxAddWords'::[WordTree]->String->Bool->[WordTree]
auxAddWords' [] y _  = [Word y]
auxAddWords' [(Word w)] y subword_occured= if w==y then [Word w] else if found ([w] ++ [y]) 0 then [subword] else if subword_occured then [Word w] else [Word w] ++ [Word y]
                        where
                            prefix = common_prefix [w] y
                            subword = if w<y then Subword prefix (construct_subword prefix ([w] ++ [y])) else Subword prefix (construct_subword prefix ([y] ++ [w]))
auxAddWords' ((Word w):wt) y subword_occured = if found ([w] ++ [y]) 0 then [subword] ++ auxAddWords' wt y True  else [Word w] ++ auxAddWords' wt y False 
                        where 
                            prefix = common_prefix [w] y
                            subword = if w<y then Subword prefix (construct_subword prefix ([w] ++ [y])) else Subword prefix (construct_subword prefix ([y] ++ [w]))

                            
auxAddWords' all@(sw@(Subword w z):wt) y subword_occured = if subword_occured 
                                                       then all else if found ([w]++[y]) 0 then if prefix == w then [Subword w (auxAddWords' z (drop ind y) False)] ++ wt  else [subword] ++ wt 
                                                                             else   if y<w then (auxAddWords' wt y False) ++ [sw] else [sw] ++(auxAddWords' wt y False)
                            where 
                            prefix = common_prefix [w] y
			    ind = length (prefix)
                            subword = Subword prefix (construct_subword2 prefix w y z)
                            

construct_subword2 prefix inner_subword_prefix newly_added wt = let ind = length(prefix); spost_prefix = drop ind inner_subword_prefix ; npost_prefix = drop ind newly_added
                                                                in if spost_prefix < npost_prefix then [Subword spost_prefix wt] ++ [Word npost_prefix]
                                                                   else [Word npost_prefix] ++ [Subword spost_prefix wt]
                            
construct_subword::String->[String]->[WordTree]
construct_subword prefix [] = []
construct_subword prefix (s:str) = let ind = length(prefix); post_str = drop ind s 
                                   in [Word post_str]
                                   ++ construct_subword prefix str


common_prefix :: [String] -> String->String
common_prefix [] _ = []
common_prefix a b = auxCommonPrefix (a++[b]) 0

auxCommonPrefix :: [String]->Int->String
auxCommonPrefix [x] _  = []
auxCommonPrefix all@(a:as) n
  | found all n = a!!n : auxCommonPrefix all (n+1)
  | otherwise = []

found :: [String] -> Int -> Bool
found [a] _ = True
found (a:as) n = if length(a)>n   then 
                                       if (a!!n == as!!0!!n) then found as n else False
                                       else False






deleteWords (Root x) y = Root (auxDeleteWords x (sort(y)))


auxDeleteWords::[WordTree]->[String]->[WordTree]
auxDeleteWords [] _ = []
auxDeleteWords x [] = x
auxDeleteWords x (y:ys) = if length(evaluated_wordtree)==0 then [] else auxDeleteWords evaluated_wordtree ys
                      where
                        evaluated_wordtree=auxDeleteWords' x y
auxDeleteWords'::[WordTree]->String->[WordTree]
auxDeleteWords' [(Word w)] y = if w==y then [] else [Word w]
auxDeleteWords' ((Word w):wt) y = if w==y then wt else [Word w] ++ auxDeleteWords' wt y
auxDeleteWords' all@(sw@(Subword w z):xs) y = if found ([w]++[y]) 0 then 
                                                                    if prefix == w then if children_count<=1 then modify_subword children w y ++ xs else [Subword w children] ++ xs
                                                                      else all
                                                                    else [sw] ++ auxDeleteWords' xs y
                      where 
                        prefix = common_prefix [w] y
                        ind = length(prefix)
                        children = auxDeleteWords' z (drop ind y)
                        children_count = length(children)


modify_subword::[WordTree]->String->String->[WordTree]
modify_subword [] subword _= [Word subword]
modify_subword [(Word w)] subword remove_str = [Word (subword++w)]
--Subword in a subword pattern matching
modify_subword all@(sw@(Subword w z):xs) subword remove_str =  if w==eval_remove_str then [] else if check_lengthall==1
                                                                                                  then [Subword (subword++w) z] else [Subword subword [Subword w z]] 
                      where
                        ind = length(subword)
                        eval_remove_str = drop ind remove_str
                        check_lengthall = length(all)




getWordsBeginWith (Root x) y = auxGetWords x y


auxGetWords::[WordTree]->String->[String]
auxGetWords [] _ = []
auxGetWords [(Word w)] y = if y == "" then [w] else  if check_begin w y then [w] else []
auxGetWords ((Word w):wt) y = if y == "" then [w] ++ auxGetWords wt y else if check_begin w y then [w] ++ auxGetWords wt y else [] ++ auxGetWords wt y
auxGetWords all@(sw@(Subword w z):xs) y = if y == "" then form_array w z ++ auxGetWords xs y else if check_begin w y then if ind <= size 
                                                                                                                          then auxGetWords' w z reduced_sword
                                                                                                                          else form_array w z 
                                                                                                                     else [] ++ auxGetWords xs y
                                                                                    where
                                                                                      reduced_sword = drop ind y
                                                                                      ind = length(w)
                                                                                      size = length(y)


auxGetWords'::String->[WordTree]->String->[String]
auxGetWords' _ [] _ = []
auxGetWords' subword [(Word w)] y = if check_begin w y then [(subword++w)] else []
auxGetWords' subword ((Word w):wt) y = if check_begin w y then [(subword++w)]  else [] ++ auxGetWords' subword wt y

form_array::String->[WordTree]->[String]
form_array y [] = []
form_array y all@[(Word w)] = [y++w]
form_array y all@((Word w):wt) = [y++w] ++ form_array y wt
form_array y (sw@(Subword w z):xs) = return_all_words (y++w) z ++ form_array y xs

return_all_words _ [] = []
return_all_words combined_last_word [(Word w)] = if w=="" then [] else [combined_last_word ++ w]
return_all_words combined_last_word ((Word w):wt) = if w=="" then return_all_words combined_last_word wt else [combined_last_word ++ w] ++ return_all_words combined_last_word wt
return_all_words combined_last_word ((Subword w z):xs) = [combined_last_word ++ w] ++ return_all_words (combined_last_word++w) z ++ return_all_words combined_last_word xs


check_begin::String->String->Bool
check_begin w y = if ind < size then string_sub == y else w==string_get
            where 
              ind = length(y)
              size = length(w)
              string_sub =  take ind w      
              string_get = take size y 