    std::vector<double> data;
    int num_na = 0;

    // statistics
    if (data.size()==0)
    {
        std::cout << "# Error: No data!" << std::endl;
        return -1;
    }
    
    double data_size = data.size();
    double data_min  = *std::min_element(std::begin(data), std::end(data));
    double data_max  = *std::max_element(std::begin(data), std::end(data));
    double data_mean = CommFunc::mean(data);
    
    std::cout << "# NumSamples = " << data_size << std::endl;
    std::cout << "# Min        = " << data_min << std::endl;
    std::cout << "# Max        = " << data_max << std::endl;
    std::cout << "# Mean       = " << data_mean << std::endl;
    

    nbin=log2(data_size)+1;
    
    std::vector<long int> hist(nbin,0);
    double bin_width = (data_max-data_min)/nbin;
    for (unsigned int idata=0; idata<data_size; idata++)
    {
        int ibin = (data[idata]-data_min)*nbin/(data_max-data_min);
        if (ibin<nbin)
            hist[ibin]++;
        else
            hist[nbin-1]++; // for data_max
    }
    
    unsigned bin_max = *std::max_element(std::begin(hist), std::end(hist));
    unsigned dot_count = 1;
    if (bin_max > maxdots)
    {
        dot_count = bin_max/maxdots;
    }
    
    std::cout << "# each " << dot_symbol << " represents a count of " << dot_count << std::endl;
    std::cout << "# --------------------------------------" << std::endl;
    for (int ibin=0; ibin<nbin; ibin++)
    {
        //std::cout << data_min+ << hist[ibin] << std::endl;
        printf(binformat.c_str(), data_min+ibin*bin_width, data_min+(ibin+1)*bin_width, hist[ibin]);
        for (int idots=0; idots<hist[ibin]/dot_count; idots++) printf("%s",dot_symbol.c_str());
        
        printf("\n");
    }
    std::cout << "# --------------------------------------" << std::endl;
    
    
    return 0;
}


