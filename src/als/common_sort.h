#pragma once

template <typename T>
void bubbleSort(T * pBuffer, int cItem, int (*pfnCompare)(const T &, const T &))
{
    for (int i = 0; i < cItem - 1; i++)
    {
        bool didSwap = false;
        for (int j = 0; j < cItem - 1 - i; j++)
        {
            T * pT0 = pBuffer + j;
            T * pT1 = pBuffer + j + 1;

            if (pfnCompare(*pT0, *pT1) > 0)
            {
                T temp = *pT0;
                *pT0 = *pT1;
                *pT1 = temp;
                didSwap = true;
            }
        }

        if (!didSwap) break;
    }
}